# Character device driver : Egoist

| Author  | Date     | Description                                                  |
| ------- | -------- | ------------------------------------------------------------ |
| Manfred | 2023/4/8 | First release : The basic character device framework is implemented |

After running `make` in the terminal, there will be one module named as follow:

- ego_char.ko

You can specify the numbers of devices that you want to create by `sudo insmod ego_char.ko count=x`, after loading the module, there will be `/dev/egoist_0, /dev/egoist_1, /dev/egoist_2 ... ...`, there will also be a class created for these devices:`/sys/class/ego_class`.

If you load the driver directly, there will be a log:`Awesome` which show you do the things upon successfully.

```shell
[Ego]ego_char_init Egoist:Awesome!
```

After you unload the module, there will be a log as follow:

```shell
[Ego]ego_char_exit Egoist:See you someday!
```

> If you don't want to see this log again, you can set `debug_option` false as below:
>
> ```c
> static bool debug_option = false;    /* hard-code control debugging */
> ```

---

Anyway, after you created the module completely, it's time to test thing with it. What you need to do is just operate the file node `/dev/egoist_x`
