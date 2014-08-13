#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

struct node {
	int x;
	struct node *next;
};

typedef struct node * lista;

void insert_last(lista *l,int n){
	if((*l)==NULL){
		lista aux = (lista) malloc(sizeof(struct node));
		aux->x = n;
		aux->next=NULL;
		(*l) = aux;
		aux = NULL;
	}else{
		insert_last(&((*l)->next),n);
	}
}

void insert_first(lista *l,int n){
	lista aux = NULL;
	aux = (lista) malloc(sizeof(struct node));
	aux->x = n;
	aux->next = (*l);
	(*l) = aux;
	aux=NULL;
}


void show_list(lista l){
	lista aux = l;
	while(aux!=NULL){
		printf("%d ",aux->x);
		aux=(*aux).next;
	}
	aux=NULL;
}


void reorganize_list(lista *l){
	if(l!=NULL){
		if((*l)!=NULL){
			lista aux1 = NULL;
			lista aux2 = (*l);
			while((*l)!=NULL){
			 	(*l) = (*l)->next;
			 	aux2->next = aux1;
			 	aux1 = aux2;
			 	aux2 = (*l);
		 	}
		 	(*l) = aux1;
		 	aux1=NULL;
		 	aux2=NULL;
	 	}
 	}
}


int main(){
	struct timeval start1,end1,start2,end2;
	lista l;
	int i =0;
	
	
	int size = 0;
	int max = 100000;
	int inc = 10;

	printf("Inserting first & reorg\n");
	for(size=inc;size<=max;size*=inc){
		printf("\n(%d elements)\t",size);
		l=NULL;
		l = (lista) malloc(sizeof(struct node));
		gettimeofday(&start1,NULL);
		for(i=0;i<size;i++){
			insert_first(&l,i);
		}
		reorganize_list(&l);
		gettimeofday(&end1,NULL);
		printf("%0.3f usec\n",(float) ((end1.tv_sec*1000000L + end1.tv_usec)-(start1.tv_sec*1000000L + start1.tv_usec))/size);
	}
	
	
	size =0;
	printf("Inserting last\n");
	for(size=inc;size<=max;size*=inc){
		printf("\n(%d elements)\t",size);
		l = NULL;
		l = (lista) malloc(sizeof(struct node));
		gettimeofday(&start2,NULL);
		for(i=0;i<size;i++){
			insert_last(&l,i);
		}
		gettimeofday(&end2,NULL);
		int aux = 0;
		for(i=0;i<size;i++){
			aux+=i;
		}
		printf("%d -- %d\n",aux,(int)pow(size,2)/2);
		printf("%0.3f usec\n",(float) ((end2.tv_sec*1000000L + end2.tv_usec)-(start2.tv_sec*1000000L + start2.tv_usec))/size );
	}
	
	printf("\n");
	return 0;
}
