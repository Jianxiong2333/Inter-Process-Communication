/*
进程间通信的信号量使用与进程同步_B
此程序执行后，即使读取同一片内存区域也应该始终显示A
*/
#include <stdio.h>
#include <string.h>
//共享内存
#include <sys/shm.h> 
//有名信号量
#include <fcntl.h>
#include <semaphore.h>
//挂起进程
#include <unistd.h>


int main(void)
{
	//创建有名信号量，并将信号量初始化为1
	sem_t* sem = sem_open("semaphore", O_CREAT, 0644, 1);
	if (SEM_FAILED == sem)//出错时返回 SEM_FAILED
	{
		perror("sem_open1 error");
	}
	int segment_id;						//分配内存标志
	char* text;							//要映射的地址
	struct shmid_ds shmbuffer;			//共享内存结构
	segment_id = shmget((key_t)1234567, 1, IPC_CREAT);	//第一个参数为内存标志，第二个参数决定了内存开辟大小，不足一页会按一页内存计算（一般是4k字节）

	int i = 1000;
	while (i--)
	{
		sem_wait(sem);					//进行P操作 信号量减1置0，阻塞其他进程请求
		text = (char*)shmat(segment_id, NULL, 0);//映射共享地址（这个每次使用需要用shmdt断开共享，因为函数的第二个参数为NULL将会在每次执行的时候随机虚拟内存地址存储地址，长期运行可能导致内存泄漏）
		memcpy(text, "B", 2);			//拷入共享地址
		usleep(10000);					//10000微妙，10毫秒
		printf("%s\n", text);			//输出当前共享内存区域地址
		shmdt(text);					//断开共享内存
		sem_post(sem);					//进行P操作 信号量加1重回1，释放阻塞
	}

	shmctl(segment_id, IPC_RMID, 0);    //释放共享内存块 
	sem_unlink("semaphore");			//释放有名信号量
	return 0;
}