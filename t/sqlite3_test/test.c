#include "../../gitpro_api/gitpro_data_api.h"
#include <stdio.h>
#include <sys/time.h>
#include "../../gitpro_api/db_constants.h"

sqlite3 *db;

int main(){
    int i = 0;
    int size = 0;
    int inc=10;
    int max = 100;
    char *query = NULL;
    char *f[] = {"ID","NAME",0};
    char *v[] = {"1","'name'",0};
    query = construct_insert("TEST",f,v);   
    printf("Insert with sqlite_exec \n"); 
    for(size=inc;size<=max;size+=inc){
  		struct timeval start,end;
  		printf("%d times\t",size);	
        gettimeofday(&start,NULL);
        for(i=0;i<size;i++){
        	exec_nonquery(query);
        }
        gettimeofday(&end,NULL);
        printf("%d ms\n",(int) (((end.tv_sec*1000L) -  (start.tv_sec*1000L))/size));
    }
    printf("Insert with sqlite3_prepare + step + finalize\n");
    for(size=inc;size<=max;size+=inc){
    	struct timeval start,end;
  		printf("%d times\t",size);	
  		sqlite3_stmt *aux = NULL;
        gettimeofday(&start,NULL);
        for(i=0;i<size;i++){
        	sqlite3_open(DB_NAME,&db);
        	sqlite3_prepare_v2(db,query,1000,&aux,0);
        	sqlite3_step(aux);
        	sqlite3_finalize(0);
        	sqlite3_close(db);
        }
        gettimeofday(&end,NULL);
        printf("%d ms\n",(int) (((end.tv_sec*1000L) -  (start.tv_sec*1000L))/size));
    }
}
