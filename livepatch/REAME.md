# 说明
io流程说明
vfs -> buffer -> Mapper layer -> Generic Block Layer -> I/O Scheduler Layer -> Block Device Driver
根据流程可以看出，可干扰的层次一共分为五个，每个层次都是异步调用。
要想影响每一个IO操作，最好的方式在VFS层在动作.
假设我们修改buffer层的end_buffer_write_sync和end_buffer_read_sync，如果数据已经在buffer存在，那么读写将不会再触发，此次IO将不存在延迟。
如果我们修改Generic Block Layer的 Generic_make__request，这是到设备层的总入口，不分读写都要经过此函数，此函数有两个限制
1. 如果cache中数据不做变动，那么此次IO无法被限制
2. 如果kernel层次走内部调用，会影响操作系统的运行速度
因此，此次IO延迟修改fs部分的IO入口.

# fs替换的函数说明,影响系统的读写接口

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf, size_t, count)

vfs读写接口较多，但是最后都是通过__vfs_read和__vfs_write操作file的
ssize_t __vfs_write(struct file *file, const char __user *p, size_t count, off_t *pos);
ssize_t __vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)

# 如何判断是否所需要的文件

linux内皆文件，设备和文件夹都是一个文件，文件具备唯一属性inode，在一个正在运行的系统中inode是唯一的，因此我们可以根据inode判别是否是需要的文件

# 编译和清理
编译
`make`
清理
`make clean`

# 使用说明
## 安装驱动
`insmod livepatch-io-latency.ko`
## 驱动卸载
```
echo 0 > /sys/kernel/livepatch/livepatch_io_latency/enabled
rmmod livepatch-io-latency
```

## 驱动使用
可以修改运行时的参数
1. 更换需要延迟的设备，可以一个文件或者设备,默认是/dev/sda
```
cat /sys/module/livepatch_io_latency/parameters/dev_name
/dev/sda
echo /dev/sdb > /sys/module/livepatch_io_latency/parameters/dev_name

```
2. 更改读延迟,单位ms,默认时0ms
```
cat /sys/module/livepatch_io_latency/parameters/read_time
0
echo 1 > /sys/module/livepatch_io_latency/parameters/read_time

```
3. 更改写延迟,单位ms,默认时0ms
```
cat /sys/module/livepatch_io_latency/parameters/write_time
0
echo 1 > /sys/module/livepatch_io_latency/parameters/write_time

```
### 测试说明
`fio --name=mytest --filename=/dev/sda --rw=randread --bs=4k --iodepth=32 --numjobs=8 --runtime=60 --group_reporting`
`fio --name=mytest --filename=/dev/sda --rw=randwrite --bs=4k --iodepth=32 --numjobs=8 --runtime=60 --group_reporting`