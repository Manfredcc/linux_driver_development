#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/mutex.h>
#include <linux/delay.h>

/*=======================================================================================
                GENRATE A PWM BREATH LIGHT

GENERAL DESCRIPTION
    This driver is intended to implement control operations for MG90S

    When              Who             What, Where, Why
    ----------        ---             ------------------------
    2023/07/17        Manfred         Initial reversion
    2023/07/20        Manfred         Add support for 360-degree version, linear speed

DETAILS
    As MG90S has two version:180-degree and 360-degree, the pwm waves for them are different,
    They both need period of 20ms, but they responses differ at duty cycle
    360-degree:
        - Valid duty cycle (ms)
            > [0.5, 1.5)-Continuously forward rotation
            > 1.5-Stop
            > (1.5.2.5]-Continuously reversal rotation
        - Speed
            > The larger the absolute difference between the duty cycle and 1.5, 
              the faster the speed
    180-degree:
========================================================================================*/
static bool debug_option = true;    /* hard-code control debugging */

#define ego_err(chip, fmt, ...)     \
    pr_err("%s: %s " fmt, chip->name,   \
        __func__, ##__VA_ARGS__)

#define ego_info(chip, fmt, ...)   \
    do {                                        \
        if (chip->debug_on && debug_option)                       \
            pr_info("%s: %s: " fmt, chip->name,    \
                __func__, ##__VA_ARGS__);   \
        else                                \
            ;   \
    } while(0)

#define MG90S_DEFAULE_PWM_PERIOD_NS   20000000 /* 20ms */
enum MG90S_ID{
    MG90S_360,
    MG90S_180,
    MG90S_MAX,
};

typedef struct _egoist {
    char *name;
    struct device *dev;
    struct class class;
    struct cdev cdev;
    dev_t devt;
    int major;
    int minor;
    struct pwm_device *pwm;
    int pwm_id;
    unsigned int period;
    unsigned int levels_max;
    unsigned int speed; /* label */
    unsigned int *levels;
    bool orientation;
    int duty_cycle;
    bool enabled;
    struct mutex ops_lock;
    
    bool debug_on;
}egoist, *pegoist;
pegoist chip = NULL;

#define PWM_BASE_VAL    1500000     /* 1.5ms */
static int compute_duty_cycle(pegoist chip)
{
    int size;
    int scale;
    int speed_label;
    int opposite_duty_cycle_ns;
    int absolute_duty_cycle_ns;

    size = chip->levels_max;
    scale = chip->levels[size];
    speed_label = chip->levels[chip->speed];
    /* opposite max - 1ms : 1 of 20 */
    opposite_duty_cycle_ns = chip->period / 20 * speed_label / scale;
    if (!chip->orientation)
        absolute_duty_cycle_ns = PWM_BASE_VAL + opposite_duty_cycle_ns;
    else
        absolute_duty_cycle_ns = PWM_BASE_VAL - opposite_duty_cycle_ns;

    return absolute_duty_cycle_ns;
}

static int pwm_update_status(pegoist chip)
{
    if (chip->speed > 0) {
        chip->duty_cycle = compute_duty_cycle(chip);
        pr_err("duty_cycle = %d, period=%d\n", chip->duty_cycle, chip->period);
        pwm_config(chip->pwm, chip->duty_cycle, chip->period);
        pwm_enable(chip->pwm);
        chip->enabled = true;
    } else {
        pwm_config(chip->pwm, PWM_BASE_VAL, chip->period);
        pwm_disable(chip->pwm);
        chip->enabled = false;
    }

    ego_info(chip, "speed was set to %u, enable=%d\n", chip->speed, chip->enabled);

    return 0;
}

int pwm_parse_360_dt(pegoist chip, struct device_node *node)
{
    struct property *prop;
    int length;
    int ret;

    prop = of_find_property(node, "speed-levels", &length);
    if (!prop)
        return -EINVAL;
    chip->levels_max = length / sizeof(u32);

    if (chip->levels_max > 0) {
        size_t size = sizeof(*chip->levels) * chip->levels_max;
        chip->levels = devm_kzalloc(chip->dev, size, GFP_KERNEL);
        if (!chip->levels)
            return -ENOMEM;
        
        ret = of_property_read_u32_array(node, "speed-levels",
                        chip->levels, chip->levels_max);
        if (ret < 0)
            return ret;
        
        chip->levels_max--; /* style: natural -> computer */
    }

    return 0;
}

int pwm_parse_180_dt(pegoist chip, struct device_node *node)
{
    return 0;
}

int pwm_parst_dt(struct device *dev, pegoist chip)
{
    struct device_node *node = dev->of_node;
    int id;
    int ret;

    if (!node)
        return -ENODEV;
    
    ret = of_property_read_u32(node, "mg90s-id", &id);
    if (ret < 0)
        return ret;
    
    switch (id) {
    case MG90S_360:
        ret = pwm_parse_360_dt(chip, node);
        break;
    case MG90S_180:
        ret = pwm_parse_180_dt(chip, node);
        break;
    default:
        ego_err(chip, "The code should not execute to this place\n");
        ret = -ENOENT;
        break;
    }
    
    if (ret) {
        return ret;
    }

    return 0;
}

/* Implement OPS-Sysfs */
static ssize_t orientation_store(struct class *c, struct class_attribute *attr, 
                    const char *buf, size_t count)
{
    int rc;
    unsigned long orientation;

    rc = kstrtoul(buf, 0, &orientation);
    if (rc)
        return rc;

    rc = mutex_lock_interruptible(&chip->ops_lock);

    if (orientation == chip->orientation) /* duplicate detection */
        return count;

    chip->orientation = !!(orientation);

    if (!chip->orientation)
        chip->speed = 0;
    
    pwm_update_status(chip);

    mutex_unlock(&chip->ops_lock);

    return count;
}

static ssize_t orientation_show(struct class *c, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", chip->orientation);
}
CLASS_ATTR_RW(orientation);

static ssize_t speed_store(struct class *c, struct class_attribute *attr, 
                    const char *buf, size_t count)
{
    int rc;
    unsigned long speed;

    rc = kstrtoul(buf, 0, &speed);
    if (rc)
        return rc;

    rc = mutex_lock_interruptible(&chip->ops_lock);

    if (speed > chip->levels_max) {
        rc = -EINVAL;
    } else {
        ego_info(chip, "set speed to %lu\n", speed);
        chip->speed = speed;
        pwm_update_status(chip);
        rc = count;
    }
    mutex_unlock(&chip->ops_lock);
    
    return rc;
}

static ssize_t speed_show(struct class *c, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", chip->speed);
}
CLASS_ATTR_RW(speed);

static struct attribute *egoist_class_attrs[] = {
    &class_attr_orientation.attr,
    &class_attr_speed.attr,
    NULL
};
ATTRIBUTE_GROUPS(egoist_class);

static const struct file_operations ego_oled_ops = {
    .owner            =   THIS_MODULE,
};

static int ego_char_init(pegoist chip)
{
    int ret;

    ret = alloc_chrdev_region(&chip->devt, 0, 1, "ego_oled");
    if (ret) {
        ego_err(chip, "Failed to allocate a char device number for [ego_oled]\n");
        return EBUSY;
    }
    chip->major = MAJOR(chip->devt);
    ego_info(chip, "major = %d\n", chip->major);

    cdev_init(&chip->cdev, &ego_oled_ops);
    chip->cdev.owner = THIS_MODULE;

    cdev_add(&chip->cdev, chip->devt, 1);

    /* Initialize ego_class, visible in /sys/class */
    chip->class.name = "egoist_class";
    chip->class.owner = THIS_MODULE;
    switch (chip->pwm_id) {
    case MG90S_360:
        chip->class.dev_groups = egoist_class_groups;
        break;
    case MG90S_180:
        chip->class.dev_groups = NULL;
    default:
        ego_err(chip, "The code should not execute to this place\n");
        ret = -ENOENT;
        break; 
    }
    
    ret = class_register(&chip->class);
    if (ret) {
        ego_err(chip, "Failed to create egoist class\n");
        unregister_chrdev_region(chip->devt, 1);

        return ret;
    }

    chip->dev = device_create(&chip->class, chip->dev,
                                    chip->devt, NULL,
                                    "%s", "pwm_mg90s");
    if (IS_ERR(&chip->dev)) {
        ego_err(chip, "Faile to create ego device\n");
        class_destroy(&chip->class);
        unregister_chrdev_region(chip->devt, 1);

        return PTR_ERR(chip->dev);
    }

    ego_info(chip, "egoist had been initialized successfully\n");
    return 0;
}

static int egoist_probe(struct platform_device *pdev)
{
    int ret = 0;

    pr_info("%s: Enter\n", __func__);

    do {
        chip = devm_kzalloc(&pdev->dev , sizeof(*chip), GFP_KERNEL);
        if (!chip) {
            pr_err("%s: failed to alloc mem for chip", __func__);
            ret = -ENOMEM;
            break;
        }
        chip->name = "ego_mg90s";
        chip->debug_on = true;
        chip->dev = &pdev->dev;
        mutex_init(&chip->ops_lock);

        ret = pwm_parst_dt(&pdev->dev, chip);
        if (ret) {
            ego_err(chip, "failed to find platform data\n");
            ret = -ENOENT;
            break;
        }

        chip->pwm = devm_pwm_get(&pdev->dev, NULL);
        if (IS_ERR(chip->pwm)) {
            ret = PTR_ERR(chip->pwm);
            if (ret == -EPROBE_DEFER)
                break;

            ego_info(chip, "unable to request PWM, trying legacu API\n");
            chip->pwm = pwm_request(chip->pwm_id, "pwm_light");
            if (IS_ERR(chip->pwm)) {
                ego_err(chip, "unable to request legacy PWM\n");
                ret = PTR_ERR(chip->pwm);
                break;
            }
        }
        
        ego_info(chip, "got pwms successfully\n");

        chip->period = pwm_get_period(chip->pwm);
        if (!chip->period) {
            chip->period = MG90S_DEFAULE_PWM_PERIOD_NS;
            pwm_set_period(chip->pwm, chip->period);
        }

        chip->speed = 0;
        chip->orientation = 0;  /* default:forward rotation */
        pwm_update_status(chip);
        ego_char_init(chip);

        platform_set_drvdata(pdev, chip);

    } while (0);

    if (ret) {
        pr_err("%s: something wrong\n", __func__);
        return ret;
    }

    ego_info(chip, "Egoist: Awesome!\n");
    return 0;
}

static int egoist_remove(struct platform_device *dev)
{
    pwm_config(chip->pwm, 0, chip->period);
    pwm_disable(chip->pwm);
    chip->enabled = false;

    if (!IS_ERR_OR_NULL(chip->dev)) {
            unregister_chrdev_region(chip->devt, 1);
            device_destroy(&chip->class, chip->devt);
        }

    if (!IS_ERR_OR_NULL(&chip->class)) {
        unregister_chrdev_region(chip->devt, 1);
        cdev_del(&chip->cdev);
        class_destroy(&chip->class);
    }

    ego_info(chip, "Egoist:See you someday!\n");

    return 0;
}

static const struct of_device_id ego_of_match[] = {
    {.compatible = "egoist, pwm-mg90s"},
    {/* Sentinel */},
};

static struct platform_driver egoist_drv = {
    .probe         = egoist_probe,
    .remove        = egoist_remove,
    .driver        = {
        .name      = "Egoist_drv",
        .owner     = THIS_MODULE,
        .of_match_table = ego_of_match,
    }
};
module_platform_driver(egoist_drv);

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implement a simple PWM breath-light");
MODULE_LICENSE("GPL");
