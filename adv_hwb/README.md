# adv_hwb

A hardware binding library for Advantech product

You need to add this code snippet into your code
```c
#include <adv_hwb.h>

if (!adv_hwb()) exit(1);
```

For some special case we need to release to code to customer, but without hardware binding function.

You can add a rule into your Makefile
```Makefile
archive: clean
	$(shell ./adv_hwb/archive.sh main.c)
```

Pass the filename which include `adv_hwb()` function to archive.sh

For more detail example, please refer this [Makefile](http://172.21.73.37:3000/MIC-3329/cpci1504_test/src/branch/master/Makefile#L17) file.
