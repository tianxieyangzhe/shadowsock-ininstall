# ˵��
io����˵��
vfs -> buffer -> Mapper layer -> Generic Block Layer -> I/O Scheduler Layer -> Block Device Driver
�������̿��Կ������ɸ��ŵĲ��һ����Ϊ�����ÿ����ζ����첽���á�
Ҫ��Ӱ��ÿһ��IO��������õķ�ʽ��VFS���ڶ���.
���������޸�buffer���end_buffer_write_sync��end_buffer_read_sync����������Ѿ���buffer���ڣ���ô��д�������ٴ������˴�IO���������ӳ١�
��������޸�Generic Block Layer�� Generic_make__request�����ǵ��豸�������ڣ����ֶ�д��Ҫ�����˺������˺�������������
1. ���cache�����ݲ����䶯����ô�˴�IO�޷�������
2. ���kernel������ڲ����ã���Ӱ�����ϵͳ�������ٶ�
��ˣ��˴�IO�ӳ��޸�fs���ֵ�IO���.

# fs�滻�ĺ���˵��,Ӱ��ϵͳ�Ķ�д�ӿ�

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf, size_t, count)

vfs��д�ӿڽ϶࣬���������ͨ��__vfs_read��__vfs_write����file��
ssize_t __vfs_write(struct file *file, const char __user *p, size_t count, off_t *pos);
ssize_t __vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)

# ����ж��Ƿ�����Ҫ���ļ�

linux�ڽ��ļ����豸���ļ��ж���һ���ļ����ļ��߱�Ψһ����inode����һ���������е�ϵͳ��inode��Ψһ�ģ�������ǿ��Ը���inode�б��Ƿ�����Ҫ���ļ�

# ���������
����
`make`
����
`make clean`

# ʹ��˵��
## ��װ����
`insmod livepatch-io-latency.ko`
## ����ж��
```
echo 0 > /sys/kernel/livepatch/livepatch_io_latency/enabled
rmmod livepatch-io-latency
```

## ����ʹ��
�����޸�����ʱ�Ĳ���
1. ������Ҫ�ӳٵ��豸������һ���ļ������豸,Ĭ����/dev/sda
```
cat /sys/module/livepatch_io_latency/parameters/dev_name
/dev/sda
echo /dev/sdb > /sys/module/livepatch_io_latency/parameters/dev_name

```
2. ���Ķ��ӳ�,��λms,Ĭ��ʱ0ms
```
cat /sys/module/livepatch_io_latency/parameters/read_time
0
echo 1 > /sys/module/livepatch_io_latency/parameters/read_time

```
3. ����д�ӳ�,��λms,Ĭ��ʱ0ms
```
cat /sys/module/livepatch_io_latency/parameters/write_time
0
echo 1 > /sys/module/livepatch_io_latency/parameters/write_time

```
### ����˵��
`fio --name=mytest --filename=/dev/sda --rw=randread --bs=4k --iodepth=32 --numjobs=8 --runtime=60 --group_reporting`
`fio --name=mytest --filename=/dev/sda --rw=randwrite --bs=4k --iodepth=32 --numjobs=8 --runtime=60 --group_reporting`