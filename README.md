# libtc

A library for serial port control, provide some API to easy the development of serial utility.

**tc = term control**

# Install
Typically we suggest use this library via `git subtree` or `git submodule` in your code.

## git submodule

The Usage of `git submodule` is in [official document](https://git-scm.com/book/en/v2/Git-Tools-Submodules)

## git subtree
### Add subtree
```
git subtree add --prefix lib http://172.21.73.37:3000/serial/libtc.git master
```

### Update subtree
Run the follow command in your code will
```
git subtree pull --prefix lib http://172.21.73.37:3000/serial/libtc.git master
```

**Add `--squash` parameter can squash the history into single commit in your repository when you using `git subtree`**

**NOTE: If you use `--squash` when add subtree, you must use `--squash` for pull too**

# API

Following API provide in this library.

```
int tc_init(char *dev);
void tc_deinit(int fd);
void tc_set_baudrate(int fd, int speed);
int tc_set_port(int fd, int databits, int stopbits, int parity);
void tc_set_rts(int fd, char enabled);
void tc_set_dtr(int fd, char enabled);
int tc_get_cts(int fd);
int tc_get_rts(int fd);
```

# Usage

1. Use `tc_init()` API to open the device, it will return a fd.
```
int fd = tc_init("/dev/ttyS0");
```

2. Use `tc_set_port()` to set port configuration
```
tc_set_port(fd, 8, 1, 0);
```

3. Use `tc_set_baudrate` to set port baudrate
```
tc_set_baudrate(fd, 115200);
```

4. You can use `write()`/`read()` to do data transmitting now.

5. In case you want to handle other signal pin, like RTS, DTR, there are also some API provided.
```
void tc_set_rts(int fd, char enabled);
void tc_set_dtr(int fd, char enabled);
int tc_get_cts(int fd);
int tc_get_rts(int fd);
```

6. After you complete your work, `use tc_deinit()` to close the device.
```
tc_deinit(fd);
```
