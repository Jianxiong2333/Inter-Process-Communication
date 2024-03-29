# Inter-Process-Communication
进程间通信的信号量使用与同步/共享内存 

进程间通信(Inter-Process Communication)即在不同的进程中对数据进行传输或者交换，本文基于内存共享(Shared Memory)中的进程同步所写，实现过程中踩了不少坑，所以分享出来一起讨(撕)论(逼)~
## 进程间通信(IPC)
IPC 的实现多种多样，管道（pipe/fifo）、套接字（socket）、共享内存，这些较为常用而共享内存是其中传输速度较快的一类，优点是能使相同设备中的不同进程访问相同一片内存完成大量数据的快速交换。并且，在其中一端无法处理信息的时候，数据仍能保存在相当于数据大小的内存区域中等待处理而不会丢失。因为操作内存本身存在极易影响程序性能的特殊性，所以在使用时更需要严格管理内存，在创建时开辟了多少内存使用完毕后就要回收多少内存，尤其是多线程通信中创建了共享内存不进行处理释放，长期的内存泄漏可能使设备本身就少得可怜的内存溢出，极可能导致程序执行的异常甚至崩溃。
## 共享内存
在 Linux 实现内存共享的函数主要有 shmget、shmat、shmdt、shmctl 这么四个。  

1、shmget 得到或者创建 一个共享内存对象  

`int shmget(key_t key, size_t size, int shmflg) `

其中 key_t key 的值为一个IPC键值，可以通过IPC_PRIVATE 建立一个新的键值为0的共享对象 ，但这并不能保证与IPC对象的对应关系，不同进程之间需要同一个且不会重复的IPC键值来完成通信，一般此参数使用ftok函数来进行创建IPC键值。  

size_t size 是映射共享内存的大小，单位为字节 (Byte)，但在创建时这里的最小分配单位是一页，大致为4KB，当你超过4KB但是小于8KB时会主动分配两页，也就是不足一页(4KB)的按照一页计算。

int shmflg 参数是需要注明的操作模式。  

- 0：取键值对应的共享内存，若此键值相应的共享内存不存在就报错。
- IPC_CREAT：存在与键值对应的共享内存则返回标识符，若不存在则创建共享内存返回标识符。
- IPC_CREAT|IPC_EXCL：不存在相应键值的共享内存则创建此共享内存，若存在则报错  

返回值：成功返回共享内存的标识符，失败返回-1。  

2、shmat 获得共享内存的地址（与进程建立映射）

`void *shmat(int shmid, const void *shmaddr, int shmflg)`  

int shmid 接收一个共享内存标识符参数，通常由 shmget 返回。  

const void *shmaddr 共享内存地址映射到进程哪块内存地址，若设 NULL 则让内核决定。  

int shmflg 选择操作模式，设 SHM_RDONLY 为只读共享内存，其他或0为可读写。  

返回值：成功返回映射完成的内存地址，失败返回-1。  

3、shmdt 与进程断开映射关系  

`int shmdt(const void *shmaddr)`  

const void *shmaddr 在进程中用于映射内存地址的指针。  

返回值：成功返回0，失败返回-1。  

4、shmctl 管理共享内存（释放共享内存）  

`int shmctl(int shmid, int cmd, struct shmid_ds *buf)`  

int shmid 接收一个共享内存标识符参数，通常由 shmget 返回。  

int cmd 是需要注明的操作模式。  

- IPC_STAT：获取共享内存状态，将共享内存的 shmid_ds 信息拷贝到 shmid_ds *buf 中。
- IPC_SET：更改共享内存状态，将 shmid_ds *buf 所指内容拷贝到共享内存 shmid_ds 中。
- IPC_RMID：删除释放这块共享内存。  

struct shmid_ds *buf 内存管理结构体，这个结构在 shm.h 中定义，网上能查到信息。  

返回值：成功返回0，失败返回-1。  
## 实际使用  
使用 shmget -> shmat -> shmctl 这样一个流程就能描述一个内存从创建到销毁的过程。  

（创建）->（映射）->（销毁）  

创建一个示例  

```c
#include <stdio.h>
#include <string.h>
//共享内存
#include <sys/shm.h> 
//挂起进程
#include <unistd.h>

int main(void)
{
	//分配内存标志
	int segment_id;
	//要映射的地址
	char* text;
	//共享内存结构
	struct shmid_ds shmbuffer;
	//创建共享内存，第一个参数内存标志，第二
	segment_id = shmget((key_t)1234567, 1, IPC_CREAT);

	//映射共享地址
	text = (char*)shmat(segment_id, NULL, 0);
	//拷入共享地址
	memcpy(text, "A", 2);
	//10000微妙，10毫秒
	usleep(10000);	
	//输出当前共享内存区域地址
	printf("%s\n", text);

	//释放共享内存块 
	shmctl(segment_id, IPC_RMID, 0);
	return 0;
}
```  

成功的读出数据，没有任何问题。  

但如果把这个场景放在一个循环里，每次循环并不立即释放内存块，而是重新映射了一次共享内存地址，毕竟这片内存区域会在循环生命周期中持续使用。  
```c
#include <stdio.h>
#include <string.h>
//共享内存
#include <sys/shm.h> 
//挂起进程
#include <unistd.h>

int main(void)
{
	//分配内存标志
	int segment_id;
	//要映射的地址
	char* text;	
	//共享内存结构
	struct shmid_ds shmbuffer;
	//创建共享内存
	segment_id = shmget((key_t)1234567, 1, IPC_CREAT); 

	while (1)
	{
		//映射共享地址
		text = (char*)shmat(segment_id, NULL, 0);
		//拷入共享地址
		memcpy(text, "A", 2);
		//10000微妙，10毫秒
		usleep(10000);
		//输出当前共享内存区域地址
		printf("%s\n", text);
	}
	//释放共享内存块 
	shmctl(segment_id, IPC_RMID, 0);
	return 0;
}
```  
加上循环后内存会以肉眼可见速度持续增长（可能我服务器内存小2333），直至程序崩溃。  

因为在 shmat 函数的第二个参数为 NULL 时，会在每次执行时在进程中随机一个虚拟内存地址存储映射地址，每次执行时不对映射地址进行释放就会造成开辟大量地址导致内存泄漏直至内存溢出。  

所以在有重复映射时流程应是（创建内存）->（映射内存）->（断开映射）->（直至销毁）  

人话来讲也就是每次执行了 shmat 以后，它并不会管之前已经映射好的内存区域，而是在下一次执行时判断到上一个地址已经被使用，而又直接在进程中又找到一个空闲地址重新开辟一个内存空间来映射地址，所以在上一次执行完毕后进行下一次映射之前就应该通过 shmdt 来断开映射，若不这样做，则每执行一次 shmat 就会产生一次内存泄漏，长此以往会出现内存溢出，导致程序Boom~  

所以在每次循环结尾之后加上一句  

`shmdt(text);`  

释放掉占用的内存地址，程序又能稳定畅快的长时间运行了。  

## 在进程间通信

事实上你直接重复打开上面的程序就可以完成进程间通信了（记得加 shmdt 函数，并且把其中一个进程 memcpy 函数中的值改为 "B" 方便观察），并且一般来说只要能保证两个进程间 shmget 函数的第一个参数 key_t key 相同，进程们都能成功访问到这片数据（补充一句：一般情况下应该用 ftok 函数来生成这个 key 值，为了方便我直接使用了1234567作为Key。  

但是这样并不能保证双方可以很好的进行通信，因为运行后你会发现这是没有规则的，两个程序在同时执行的时候，你甚至可能都没法保证在下一次执行时一定能读到自己上一次才存入进去的值，因为你读到的值已经被对方进程修改了，甚至你们可能在同时读写同一片内存区域，这样的情况我没遇到过，但估计会崩溃。
## 信号量

我们通常需要一个东西来指示一个共享内存的读写状态，至少在一个进程在进行”读操作“的时候另一个进程不会抢在另一个进程读之前先写入数据（因为这样被覆盖信息后至少会少读一个数据）。所以需要一个信号量就像指示灯一样，指示调度不同进程的操作，而进行协调两个对象之间的阻塞互斥关系的过程就叫同步。  

如果我要获得这个内存资源（加锁），则是 P(S) 操作，S 是信号量， P将信号量的值-1。  

如果我要释放这个内存资源（解锁），则是 V(S) 操作，S 是信号量，V将信号量的值+1。  

那么在进行 P 操作 的时候，S <= 0 时将会阻塞操作，等待 S >= 1 继续执行，假设 S 被初始化为1，在 A进程 将 S 值减 1得到 0 ，在 B 程序也在请求获得资源进行 P操作 时 S 为 0，进程被阻塞，需要等待 A进程 使用 V操作 释放资源将 S+1 使其大于0，则阻塞结束。
信号量分为”无名信号量“和”有名信号量“

其中无名信号量是存储在内存中协调单个进程多线程之间的同步问题，数据存放在内存中，有名信号量数据存放在 /dev/shm/ 目录文件中，以 sem_open 创建的有名信号量名称为后缀。  

有名信号量使用包含了 **sem_open、sem_wait、sem_post、sem_close、sem_unlink** 几个函数。  

1、sem_open 创建或打开一个有名信号量  

`sem_t *sem_open(const char *name,int oflag,mode_t mode,unsigned int value);` 

const char *name 信号量的名称，会在 /dev/shm 里以文件后缀名形式体现。  

int oflag 选择操作方式  

- O_CREAT：没有指定的信号量就创建一个信号量，有指定信号量不报错
- O_CREAT|O_EXCL：如果没有指定的信号量就创建，有指定信号量就报错  

mode_t mode 文件权限  

- 777：所有用户都可以读、写、执行
- 666：所有用户都可以读、写
- 711：拥有者可读、写、执行，其他人只能执行
- 等还有很多，按照 Linux 权限定义规则填写即可  

返回值：成功返回信号量指针，失败返回 SEM_FAILED。  

value 信号量初始值，这个值如果是做互斥使用，建议为1  

2、sem_wait 执行一个 P 操作（对信号量值-1）  

`int sem_wait(sem_t * sem);`  

sem_t * sem 为信号量指针。  

如信号量值S>0，则S-1，如 S=0 则线程阻塞，等待 S>0 后继续执行，用于获得资源。  

返回值：成功返回0，失败返回-1。  

3、sem_post 执行一个 V 操作（对信号量值+1）  

`int sem_post(sem_t *sem);`  

sem_t * sem 为信号量指针。  

对信号量S进行+1操作，用于释放资源。  

返回值：成功返回0，失败返回-1。  

4、sem_close sem_unlink  

`int sem_close(sem_t *sem);`  

sem_t * sem 为信号量指针。  

返回值：成功返回0，失败返回-1。  

5、sem_unlink 删除一个有名信号量文件  

int sem_unlink(constchar*name);  

## 什么是同步  

首先要实现同步，要先了解同步是什么，在维基百科关于”同步（计算机科学）“是这样解释的：”进程同步指多个进程在特定点会合（join up）或者握手使得达成协议或者使得操作序列有序“，也就是保证多个进程之间对资源的访问是有序的，有理的。不是不受先后顺序控制的。  

要实现同步就要实现互斥，而什么是互斥？在维基百科关于”互斥（逻辑学）“解释到：”互斥是一种逻辑关系，指几个变量或事件之中的任一个不可能与其它一个或多个同时为真，或同时发生的情况“，也就是保证资源竞争之间不能同时进行资源操作，一个在操作另一个就要等待，就像上厕所一样，有人在使用洗手间，其他人就要等待他使用完毕后才能去使用洗手间。  

这里还要牵涉到一个临界区域的概念，也就是上面提到的厕所，就是一个临界区。如果这些功能在同一时间只能被一个进程进行调控，那么这个代码块的内容就是临界区。  

## 实现有序访问  

在进入临界区之前对资源进行锁定（P 操作），在出临界区的时候进行释放（V 操作）就完成了对一个临界区的资源调控。  

```
信号量s=1
p操作（s(1)-1=0）锁定资源
{
   临界区
   互斥操作
}
v操作（s(0)+1=1）释放资源
```
这个示例就不会再出现ABAAAAABAAAB这种情况（读取到非自身进程写入的数据，同步访问资源），而是像 AAAAAAAAAAAA 或者 BBBBBBBBBBBB 这样仅访问到自己写入的数据（有序，互斥访问资源），如果要实现两个进程之间进行数据的交换可以自己摸索一下。
