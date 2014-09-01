#include "gitpro_data_api.h"

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

/* 	Name		: logic_cond
	Parameters	: two conditions and logic operator to apply
	Return		: conditions with logic operator applied
	Example		: logic_cond(a,' and ',b) returns 'a and b' */
char *logic_cond(char *cond1,char *logic,char *cond2){
	int size = strlen(cond1)+strlen(cond2);
	/* Allocates memory to ' and ' or ' or ', conditions, 2 parenthesis
	  	(at start and end to make possible group logic conditions) and 
		end string character */
	char *result = (char *) malloc(strlen(logic)+size+2+1);
	strcpy(result,"(");
	strcat(strcat(strcat(strcat(result,cond1),logic),cond2),")");
	return result;
}

/*	Name		: arraylen
	Parameters	: array of elements
	Return		: length of array */
int arraylen(char **array){
	char **aux = array;
	int i =0;
	if(aux!=NULL){
		while(aux[i]!=NULL){
			i++;
		}
	}
	aux=NULL;
	return i;
}


/*	Name		: memory_to_alloc
	Parameters	: array of parameters
	Returns		: number of necesary bytes to allocate */
int memory_to_alloc(char *params[]){
	int num_params = arraylen(params);
	int i=0;
	int size =0;
	for(i=0;i<num_params;i++){
		size+=(strlen(params[i])*sizeof(char));
	}
	return size;
}

/* 	Name		: make_scalar
	Parameters	: name of expresion and field
	Return		: string with expresion applied to field or null y expression or field is null
	Used for	: avoid duplicate code in scalar functions */
char *make_scalar(char *exp,char *f){
	if(exp==NULL || f==NULL){
		return NULL;
	}	
	/* Allocate memory to expression, field, 2 parenthesis and end string character */
	char *scalar = (char *) malloc(strlen(exp)+strlen(f)+2+1);
	strcpy(scalar,exp);
	strcat(strcat(strcat(scalar,"("),f),")");
	return scalar;
}

/*	Name		: add_commas
	Parameters	: original string, number of fields and fields to separate with commas
	Return		: original string changed with fields added and commas separated */
void add_commas(char *orig,int num,char *f[]){
	int i =0;
	for(i=0;i<num;i++){
		strcat(orig,f[i]);
		if(i!=(num-1)){
			strcat(orig,",");
		}
	}
}


/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

/* See specification in gitpro_data_api.h */
char *format_number(int number){
	//Allocate 10 bytes because it's maximum lenght of an int. Add 1 to end string char
	char *converted = (char *) malloc(10+1);
	sprintf(converted,"%d",number);
	return converted;
}

/* See specification in gitpro_data_api.h */
char *format_string(char *orig){
	char *rn =(char *) malloc(strlen(orig)+2+1);
	strcpy(rn,"'");
	strcat(strcat(rn,orig),"'");
	return rn;
}

/* See specification in gitpro_data_api.h */
int new_task_id(){
	char *c = max(TASK_ID);
	char *fields[] = {c,0};
	char *query = (char *) construct_query(fields,from(TASK_TABLE),NULL,NULL,NULL);
	int task_id = exec_int_scalarquery(query);
	task_id++;
	free(query);
	free(c);
	return task_id;
}

/* See specification in gitpro_data_api.h */
char *construct_delete(char *table,char *where){
	if(table==NULL){
		return NULL;
	}	
	const char *start = "DELETE FROM ";
	int where_size = 0;
	if(where!=NULL){
		where_size = strlen(where);
	}
	/* Allocates memory to 'delete from ', table name, where part if not null and
	 	end string character */
	char *delete = (char *) malloc(strlen(start)+strlen(table)+where_size+1);
	strcpy(delete,start);
	strcat(delete,table);
	if(where!=NULL){
		strcat(delete,where);
		free(where);
	}
	return delete;
}

/* See specification in gitpro_data_api.h */
char *construct_update(char *table,char *asig[],char *where){
	if(table==NULL || asig==NULL){
		return NULL;
	}	
	const char *start = "UPDATE ";
	const char *mid = " SET ";
	int size = memory_to_alloc(asig);
	int where_size = 0;
	if(where!=NULL){
		where_size = strlen(where);	
	}
	int num_asig = arraylen(asig);
	/* Allocates memory to 'update ', ' set ', where part if not null, asignations,
	 	table name and separate asig commas. Finally adds 1 to end string character */
	char *update = (char *) malloc(strlen(start)+strlen(mid)+size+
									where_size+strlen(table)+(num_asig-1)+1);
	strcpy(update,start);
	strcat(strcat(update,table),mid);
	add_commas(update,num_asig,asig);
	int i=0;
	for(i=0;i<num_asig;i++){
		free(asig[i]);
	}
	if(where!=NULL){
		strcat(update,where);
		free(where);
	}
	return update;
}

/* See specification in gitpro_data_api.h */
char *asig(char *field,char *value){
	if(field==NULL || value==NULL){
		return NULL;
	}
	const char *simb = "=";
	/* Allocates memory to symbol, field and value. Finally adds 1 to end string character */
	char *asignation = (char *) malloc(strlen(simb)+strlen(field)+strlen(value)+1);
	strcpy(asignation,field);
	strcat(strcat(asignation,simb),value);
	return asignation;
}

/* See specification in gitpro_data_api.h */
char *construct_insert(char *table,char *fields[],char *values[]){
	if(table==NULL || fields==NULL || values==NULL){
		return NULL;
	}	
	const char *start = "INSERT INTO ";
	const char *mid = " VALUES(";
	const char *end = ")";
	int size = memory_to_alloc(fields);
	size += memory_to_alloc(values);
	int num_fields = arraylen(fields);
	int num_values = arraylen(values);
	/* Allocates memory to 'insert into ', 'values(', ')', 2 parenthesis, length of all fields
		and all values. In addition, separate commas for fields and values, 
		length of table name. Finally, one extra space to end string character */
	char *insert = (char *) malloc(strlen(start)+strlen(mid)+strlen(end)+2+strlen(table)
								+size+(num_fields-1)+(num_values-1)+1);
	strcpy(insert,start);
	strcat(strcat(insert,table),"(");
	add_commas(insert,num_fields,fields);
	strcat(strcat(insert,")"),mid);
	add_commas(insert,num_values,values);	
	strcat(insert,end);
	return insert;
}

/* See specification in gitpro_data_api.h */
char *construct_query(char *fields[],char *from,char *where,char *group_by,char *order_by){
	if(fields==NULL || from==NULL){
		return NULL;
	}
	const char *select="SELECT ";
	int size = memory_to_alloc(fields);
	int num_fields = arraylen(fields);
	int where_size = 0;
	int group_size = 0;
	int order_size = 0;
	if(where!=NULL){
		where_size = strlen(where);
	}
	if(group_by!=NULL){
		group_size = strlen(group_by);
	}
	if(order_by!=NULL){
		order_size = strlen(order_by);
	}
	/* Allocates memory to 'select ', length of fields to select, separate commas
		for fields, from part, where part, group by part, order by part and 
		end string character */
	char *query = (char *) malloc(strlen(select)+size+(num_fields-1)
								+strlen(from)+where_size+group_size+order_size+1);
	strcpy(query,select);
	add_commas(query,num_fields,fields);
	strcat(query,from);
	free(from);
	if(where!=NULL){
		strcat(query,where);
		free(where);	
	}
	if(group_by!=NULL){
		strcat(query,group_by);
		free(group_by);
	}
	if(order_by!=NULL){
		strcat(query,order_by);
		free(order_by);
	}
	return query;
}

/* See specification in gitpro_data_api.h */
char * from(char *table_name){
	if(table_name==NULL){
		return NULL;
	}
	const char *from = " FROM ";
	int size = strlen(table_name);
	/* Allocates memory to ' from ', table name and end string character */
	char *result = (char *) malloc(strlen(from)+size+1);
	strcpy(result,from);
	strcat(result,table_name);
	return result;
}

/* See specification in gitpro_data_api.h */
char *where(char *conds){
	const char *where = " WHERE ";
	/* Allocates memory to ' where ', conditions and end string character */
	char *result = (char *) malloc(strlen(where)+strlen(conds)+1);
	strcpy(result,where);
	strcat(result,conds);
	free(conds);
	return result;
}

/* See specification in gitpro_data_api.h */
char *group_by(char *fields[]){
	if(fields==NULL){
		return NULL;
	}
	const char *group = " GROUP BY ";
	int num = arraylen(fields);
	/* Allocates memory to ' group by ', fields, separate commas and end string character */
	char *result = (char *) malloc(strlen(group)+memory_to_alloc(fields)+(num-1)+1);
	strcpy(result,group);
	add_commas(result,num,fields);
	return result;
}

/* See specification in gitpro_data_api.h */
char *order_by(char *fields[],int inverse){
	if(fields==NULL){
		return NULL;
	}
	const char *order = " ORDER BY ";
	const char *type = " DESC ";	
	int num = arraylen(fields);
	int type_length = 0;
	if(inverse){
		type_length = strlen(type);
	}
	/* Allocates memory to ' order by ', fields, separate commas, order type if specified
	 	and end string character */
	char *result = (char *) malloc(strlen(order)+memory_to_alloc(fields)+(num-1)
								+type_length+1);
	strcpy(result,order);
	add_commas(result,num,fields);
	if(inverse){
		strcat(result,type);
	}
	return result;
}

/* See specification in gitpro_data_api.h */
char *cond(char *field1,char *comp,char *field2){
	if(field1==NULL || comp==NULL || field2==NULL){
		return NULL;
	}
	/* Allocates memory to fields to compare, comparation operator and
		end string character */
	char *result = (char *) malloc(strlen(field1)+strlen(field2)+strlen(field2)+1);
	strcpy(result,field1);
	strcat(strcat(result,comp),field2);
	return result;
}

/* See specification in gitpro_data_api.h */
char *_and_(char *cond1,char *cond2){
	if(cond1==NULL || cond2==NULL){
		return NULL;
	}	
	char *and = " AND ";
	return logic_cond(cond1,and,cond2);
}

/* See specification in gitpro_data_api.h */
char *_or_(char *cond1,char *cond2){
	if(cond1==NULL || cond2==NULL){
		return NULL;
	}
	char *or = " OR ";
	return logic_cond(cond1,or,cond2);
}

/* See specification in gitpro_data_api.h */
char *count(char *field){
	return make_scalar("COUNT",field);
}

/* See specification in gitpro_data_api.h */
char *avg(char *field){
	return make_scalar("AVG",field);
}

/* See specification in gitpro_data_api.h */
char *min(char *field){
	return make_scalar("MIN",field);
}

/* See specification in gitpro_data_api.h */
char *max(char *field){
	return make_scalar("MAX",field);
}

/* See specification in gitpro_data_api.h */
char *sum(char *field){
	return make_scalar("SUM",field);
}

/* See specification in gitpro_data_api.h */
char *lower(char *field){
	return make_scalar("LOWER",field);
}

/* See specification in gitpro_data_api.h */
char *upper(char *field){
	return make_scalar("UPPER",field);
}

/* See specification in gitpro_data_api.h */
char *concat(char *string1, char *string2){
	/* string 1 length + string 2 lenght + end string character + 2 parenthesis + 2 spaces 
	+ concat operator (||) + coalesce operator, other 2 parenthesis, 1 comma and empty char 
	(+13)*/
	char *result = (char *) malloc(strlen(string1)+strlen(string2)+1+2+1+2+13);	
	strcpy(result,"(");
	strcat(result,"coalesce(");
	strcat(result,string1);
	strcat(result,",'')");
	strcat(result," || ");
	strcat(result,string2);
	strcat(result,")");
	return result;
}
