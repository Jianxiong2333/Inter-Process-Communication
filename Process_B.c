/*
���̼�ͨ�ŵ��ź���ʹ�������ͬ��_B
�˳���ִ�к󣬼�ʹ��ȡͬһƬ�ڴ�����ҲӦ��ʼ����ʾA
*/
#include <stdio.h>
#include <string.h>
//�����ڴ�
#include <sys/shm.h> 
//�����ź���
#include <fcntl.h>
#include <semaphore.h>
//�������
#include <unistd.h>


int main(void)
{
	//���������ź����������ź�����ʼ��Ϊ1
	sem_t* sem = sem_open("semaphore", O_CREAT, 0644, 1);
	if (SEM_FAILED == sem)//����ʱ���� SEM_FAILED
	{
		perror("sem_open1 error");
	}
	int segment_id;						//�����ڴ��־
	char* text;							//Ҫӳ��ĵ�ַ
	struct shmid_ds shmbuffer;			//�����ڴ�ṹ
	segment_id = shmget((key_t)1234567, 1, IPC_CREAT);	//��һ������Ϊ�ڴ��־���ڶ��������������ڴ濪�ٴ�С������һҳ�ᰴһҳ�ڴ���㣨һ����4k�ֽڣ�

	int i = 1000;
	while (i--)
	{
		sem_wait(sem);					//����P���� �ź�����1��0������������������
		text = (char*)shmat(segment_id, NULL, 0);//ӳ�乲���ַ�����ÿ��ʹ����Ҫ��shmdt�Ͽ�������Ϊ�����ĵڶ�������ΪNULL������ÿ��ִ�е�ʱ����������ڴ��ַ�洢��ַ���������п��ܵ����ڴ�й©��
		memcpy(text, "B", 2);			//���빲���ַ
		usleep(10000);					//10000΢�10����
		printf("%s\n", text);			//�����ǰ�����ڴ������ַ
		shmdt(text);					//�Ͽ������ڴ�
		sem_post(sem);					//����P���� �ź�����1�ػ�1���ͷ�����
	}

	shmctl(segment_id, IPC_RMID, 0);    //�ͷŹ����ڴ�� 
	sem_unlink("semaphore");			//�ͷ������ź���
	return 0;
}