#include<stdio.h>
#include<math.h>
#include<stdlib.h>
#include<pthread.h>
#include<time.h>
#include<sys/time.h>
#define BUF_SIZE 6
#define CHUNK_SIZE 19
int x, y, n, t;
long int e[50],d[50];

int len = 0;
int head = 0;
int tail = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // needed to add/remove data from the buffer
pthread_mutex_t mutex_buf[BUF_SIZE]={PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};//each index of buffer has one mutex to read/write it
pthread_cond_t can_produce = PTHREAD_COND_INITIALIZER; // signaled when items are removed
pthread_cond_t can_consume = PTHREAD_COND_INITIALIZER; // signaled when items are added

typedef struct packet{
	long int data[CHUNK_SIZE+1];
	int id;
}packet;
typedef struct result{//use to keep result of each consumer and finally we move data to output array
	int id;
	char *data;
}result;
typedef struct argument{
	int num_packet;
	int num_threadConsumer;
	int num_threadProducer;
	int tid;
	char* out1;
	char* input;
	int num_input;
	result* local_buf;
	int result_count;
}argument;
packet buf[BUF_SIZE];

long int cd(long int a)
{
        long int k = 1;
        while(1)
        {
                k = k + t;
                if(k % a == 0)
                        return(k / a);
        }
}


int prime(long int pr)
{
	int i;
	int j = sqrt(pr);
	for(i = 2; i <= j; i++)
	{
		if(pr % i == 0)
			return 0;
	}
	return 1;
}
void decrypt(long int *en,char *m)
{
        long int pt, ct, key = d[0], k;
        int i = 0;
        while(en[i] != -1)
        {
                ct = en[i]-96;//we subtract 96 because we add 96 in the encrypt function // for change it to ascii code
                k = 1;
                for(int j = 0; j < key; j++)
                {
                        k = k * ct;
                        k = k % n;
                }
                pt = k + 96;
                m[i] = pt;
                i++;
        }
        m[i] = -1;

}
void encrypt(int start,int length,char *m,long int *en)
{
        long int pt, ct, key = e[0], k, len;
        int i = start;
        len = start+length;
        while(i != len)
        {
                pt = m[i];
                pt = pt - 96;
                k = 1;
                for(int j = 0; j < key; j++)
                {
                        k = k * pt;
                        k = k % n;
                }
                ct = k + 96;
                en[i-start] = ct;
                i++;
        }
        en[i-start] = -1;
}
void encryption_key()
{
        int k;
	int flag;
        k = 0;
        for(int i = 2; i < t; i++)
        {
                if(t % i == 0)
                        continue;
                flag = prime(i);
                if(flag == 1 && i != x && i != y)
                {
                        e[k] = i;
                        flag = cd(e[k]);
                        if(flag > 0)
                        {
                                d[k] = flag;
                                k++;
                        }
                        if(k == 99)
                                break;
                }
        }
}
void* producer(void *arg) {
	argument targ=*(argument*)arg;
	int num_chunk=targ.num_packet;
	int num_thread=targ.num_threadProducer;
	int id=targ.tid;
	int num_input=targ.num_input;
	char* input=targ.input;
	int si=(num_chunk/num_thread)*id;
	int ei=si+(num_chunk/num_thread);
	if(id==num_thread-1){//if number of packet doesnt divisible by number of thread we should encrypt the end packet
		ei=num_chunk;
	}
	int lastHead=0;

	long int * encode=(long int *)malloc((CHUNK_SIZE+1)*sizeof(long int));
	for(int i=si;i<ei;i++){
			encrypt(i*CHUNK_SIZE,CHUNK_SIZE,input,encode);
			pthread_mutex_lock(&mutex);
        	while(len == BUF_SIZE) { // full
            		pthread_cond_wait(&can_produce, &mutex);
        	}
        	lastHead=head;//we keep head for transfer data to buffer after critical area
			head = (head + 1)%BUF_SIZE;
			len++;
			pthread_mutex_lock(&mutex_buf[lastHead]);
//        	pthread_cond_signal(&can_consume);
        	pthread_mutex_unlock(&mutex);
	        for(int j=0;j<CHUNK_SIZE+1;j++){	
			buf[lastHead].data[j]=encode[j];
			}
			buf[lastHead].id=i;
        	pthread_cond_signal(&can_consume);
			pthread_mutex_unlock(&mutex_buf[lastHead]);
	}
	if(id ==num_thread-1){//if number of character in input doesnt divisible by chunk size
		if(num_input%CHUNK_SIZE!=0){
			si=num_chunk*CHUNK_SIZE;
			ei=num_input;
			long int * encode=(long int *)malloc((CHUNK_SIZE+1)*sizeof(long int));
			encrypt(si,ei-si,input,encode);
			pthread_mutex_lock(&mutex);
        	while(len == BUF_SIZE) { // full
            		pthread_cond_wait(&can_produce, &mutex);
        	}	
			lastHead=head;        		
			head = (head + 1)%BUF_SIZE;
			len++;
			pthread_mutex_lock(&mutex_buf[lastHead]);
			pthread_cond_signal(&can_consume);
        	pthread_mutex_unlock(&mutex);
	        for(int j=0;j<CHUNK_SIZE+1;j++){	
				buf[lastHead].data[j]=encode[j];
			}
			buf[lastHead].id=num_chunk;
			pthread_mutex_unlock(&mutex_buf[lastHead]);
		}
	}

   free(encode);
   pthread_exit(NULL);
}
void* consumer(void *arg) {
	argument targ=*(argument*)arg;
	int num_packet=targ.num_packet;
	int num_thread=targ.num_threadConsumer;
	int id = targ.tid;
	int num_input=targ.num_input;
	int count=num_packet/num_thread;
	if(id==num_thread-1){
		if(num_packet%num_thread!=0){
			count+=(num_packet-(count * num_thread));
		}
		if(num_input%CHUNK_SIZE!=0){
			count++;
		}
	}
	result *local_buf=(result*)malloc(count*sizeof(result));
	int counter=0;
	int lastTail=0;
	long int *tmp=malloc((CHUNK_SIZE+1)*sizeof(long int));
	for(int i=0;i<count;i++){
		char* decode=(char*)malloc((CHUNK_SIZE+1)*sizeof(char));
		int packetId;
		pthread_mutex_lock(&mutex);
        	while(len == 0) { // empty
            		pthread_cond_wait(&can_consume, &mutex);
        	}
		lastTail=tail;
        tail = (tail + 1)%BUF_SIZE;
		len--;	
		pthread_mutex_lock(&mutex_buf[lastTail]);
//		pthread_cond_signal(&can_produce);
        pthread_mutex_unlock(&mutex);
		packetId=buf[lastTail].id;
		for(int j=0;j<CHUNK_SIZE+1;j++){
			tmp[j]=buf[lastTail].data[j];
		}
		pthread_cond_signal(&can_produce);
		pthread_mutex_unlock(&mutex_buf[lastTail]);
		decrypt(tmp,decode);
		local_buf[counter].id=packetId;//keep this decode file that finally main thread write it in output array
		local_buf[counter].data=decode;
		counter++;
		
	}
	argument* targ2=(argument*)arg;
	targ2->local_buf=local_buf;
	targ2->result_count=counter;	
  	free(tmp);
    pthread_exit(NULL);
}
void validate(char* a,char *b,int size){
	for(int i=0;i<size;i++){
		if(a[i]!=b[i]){
			printf("different value in index:%d correct value:%c worng value:%c\n",i,a[i],b[i]);
			return ;
		}
	}
	printf("complete equal\n");
}
int main(int argc,char* argv[]){
	struct timeval start, end;
			
	if(argc!=3){	
		printf("wrong command :\n correct way to execute this ptogram:\n ./your-encryption-program   number-of-thread   input-file-path\n");
		return 0;
	}
	int num_thread=atoi(argv[1]);
	char* path=argv[2];
	FILE* file_path;
	file_path=fopen(path,"r");
	int c;
	int count=0;
	char *input;
	fseek(file_path,0L,SEEK_END);
	int size=ftell(file_path);
	input=(char*)malloc((size+2)*sizeof(char));
	fseek(file_path,0L,SEEK_SET);
	while((c=getc(file_path))!=EOF){
		input[count]=c;
		count++;
	}
	fclose(file_path);
	x = 19; //first prime number
	y = 11; //second prime number
	n = x * y;
	t = (x-1) * (y-1);
	encryption_key();
	char * out1=(char*)malloc((size+2)*sizeof(char));
	gettimeofday(&start, NULL);
	argument arg;
	arg.num_packet=(count/CHUNK_SIZE);
	arg.num_threadConsumer=1;
	arg.num_threadProducer=1;
	arg.tid=0;
	arg.input=input;
	arg.num_input=count;
	arg.out1=out1;
        pthread_t prod_1;
	pthread_t cons_1;
	pthread_create(&prod_1, NULL, producer, (void*)&arg);
    pthread_create(&cons_1, NULL, consumer, (void*)&arg);
    pthread_join(prod_1, NULL);
    pthread_join(cons_1, NULL);
//////////////write result of each consumer thread in output array
		int counter1=arg.result_count;
		for(int j=0;j<counter1;j++){

			for(int k=0;k<CHUNK_SIZE ;k++){
				if(arg.local_buf[j].data[k]!=-1){
					out1[(arg.local_buf[j].id*CHUNK_SIZE)+k]=arg.local_buf[j].data[k];
				}else{
					break;
				}
			}
			free(arg.local_buf[j].data);
		}
		free(arg.local_buf);		
//////////////
	gettimeofday(&end, NULL);
	printf("final output serial:\n");
    double diff = (end.tv_sec - start.tv_sec) * 1000000.0 +(end.tv_usec - start.tv_usec);
    printf("serial time calculation duration: %.5fms\n", diff / 1000);	
	validate(input,out1,count);
	
	int num_producer=num_thread/2;
	int num_consumer=num_thread/2;
	pthread_t t_producer[num_producer];
	pthread_t t_consumer[num_consumer];
	argument arg_producer[num_producer];
	argument arg_consumer[num_consumer];
	gettimeofday(&start, NULL);	
	for(int  i=0;i<num_producer;i++){
		arg_producer[i].num_packet=(count/CHUNK_SIZE);
		arg_producer[i].num_threadProducer=num_producer;
		arg_producer[i].tid=i;
		arg_producer[i].input=input;
		arg_producer[i].num_input=count;
		pthread_create(&t_producer[i],0,producer,(void*)&arg_producer[i]);
	}	
	char * out2=(char*)malloc((size+2)*sizeof(char));
	for(int i=0;i<num_consumer;i++){
		arg_consumer[i].num_packet=(count/CHUNK_SIZE);	
		arg_consumer[i].num_threadConsumer=num_consumer;
		arg_consumer[i].tid=i;
		arg_consumer[i].input=input;
		arg_consumer[i].num_input=count;
		arg_consumer[i].out1=out2;
		pthread_create(&t_consumer[i],0,consumer,(void*)&arg_consumer[i]);
	}
	int status;
    	for (int i = 0; i < num_producer; ++i) {
        	pthread_join(t_producer[i], (void *)&status);
		}
    	for (int i = 0; i < num_consumer; ++i) {
        	pthread_join(t_consumer[i], (void *)&status);
		int counter=arg_consumer[i].result_count;
		for(int j=0;j<counter;j++){

			for(int k=0;k<CHUNK_SIZE ;k++){
				if(arg_consumer[i].local_buf[j].data[k]!=-1){
					out2[(arg_consumer[i].local_buf[j].id*CHUNK_SIZE)+k]=arg_consumer[i].local_buf[j].data[k];
				}else{
					break;
				}
			}
			free(arg_consumer[i].local_buf[j].data);
		}
		free(arg_consumer[i].local_buf);		
   	}
	gettimeofday(&end, NULL);
    	diff = (end.tv_sec - start.tv_sec) * 1000000.0+ (end.tv_usec - start.tv_usec);
    	printf("Parallel  time calculation duration: %.5fms\n", diff / 1000);
	printf("final output parallel:\n");
	validate(input,out2,count);
	free(out2);
	free(out1);
	free(input);

	return 0;
}
