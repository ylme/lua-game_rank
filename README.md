
## Compile

* install lua lib

```shell
$ make
```

## Test with lua5.3

* base test

```shell
$ lua test.lua
```

* stress test

```shell
$ lua stress_test.lua
```

## Problems

* stress test 指定 `N` 为 10w 时，我的机器上执行大概 1 秒多，当把 `N` 改成 50w 时，再执行就需要 50 秒多，为什么不是 5 秒多执行完呢（这里的时间是 2020年5月4日更新采用 `memmove` 前的测试记录）。
* stress test 把 `obj` 设置为 `nil` 强制垃圾回收，内存似乎只回收了一半，相比初始时还增加了很多内存，为什么回收后没有回到初始的内存呢。
* stress test 执行 100w 条结果能否优化到 2 秒内呢。
