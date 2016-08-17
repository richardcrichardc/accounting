#define _XOPEN_SOURCE 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <readline/readline.h>

/******** TYPES ********/

typedef char bool;
#define true 1
#define false 0


struct account_t {
	int id;
	int depth;
	bool positive;
	char* name;
	struct split_t* first_split;
	struct split_t* last_split;
	int balance;
	struct account_t* next;
	struct account_t* sub_accounts;
};

struct transaction_t {
	time_t date;
	char* desc;
	struct split_t* splits;
	struct transaction_t* next;
};

struct split_t {
	int amount;
	struct transaction_t* transaction;
	struct account_t* account;
	char* desc;
	struct split_t* next_transaction_split;
	struct split_t* next_account_split;
};

typedef struct {
  char *text;
  void (*fun)();
} Command;


typedef struct account_t Account;
typedef struct transaction_t Transaction;
typedef struct split_t Split;

#define OK 0
#define TOO_LONG 1
#define GARBAGE 2
#define TOO_MANY_DECIMALS 3
#define BAD 4

/******** GLOBALS ********/

int argc;
char **argv;

int lineno;
#define PARSE_BUF_SIZE 200
char parse_buf[PARSE_BUF_SIZE];

char* filename;
FILE* f;
Account* first_account=NULL;
Transaction* first_transaction=NULL;
Transaction* last_transaction=NULL;

//import
time_t csv_date;
Account* csv_account;
char csv_desc[200];
char csv_ref[200];
int csv_amount;
FILE* journal;

/******** FUNCTIONS ********/

void help()
{
	printf("Usage: acc <command> ...\n");
	printf("Commands:\n");
	printf("  accounts - print chart of accounts\n");
	printf("  verify - verify journal and accounts data file\n");
	printf("  import <acc#> <csv file> - import transactions from csv\n");
	printf("  summary - print account balances\n");
	printf("  statement <acc#> - print account statement\n");
	printf("  statement-splits <acc#> - print extended account statement\n");
	printf("  download-dates <acc#> - generate date string for kiwibank download\n");
	printf("  invoice <cust#> <date> <desc> <amount> - enter invoice into accounts receivable\n");
}

void die(char* message)
{
	fprintf(stderr,"%s\n", message);
	exit(1);
}

void parse_die(char* message)
{
	fprintf(stderr,"%s line %i: %s\n",filename,lineno,message);
	exit(1);

}

void parse_die2(char* message)
{
	fprintf(stderr,"%s line %i '%s': %s\n",filename,lineno,parse_buf,message);
	exit(1);
}

void *my_malloc(size_t size)
{
	void *m=malloc(size);
	if (!m)
		die("Out of memory");
}

char *my_strdup(char* string){
	size_t size=strlen(string)+1;
	char *string2=(char*)my_malloc(size);
	memcpy(string2,string,size);
	return string2;
}


void open_datafile(char* name)
{
	filename=name;
	f=fopen(name,"r");
	lineno=1;
	if (!f) {
		fprintf(stderr,"Couldn't open %s\n",name);
		exit(1);
	}
}

void next_word()
{
	int pos=0;
	char c;
	while (1){
		c=getc(f);
		if (feof(f))
				parse_die("Unexpected EOF");
		if (pos==PARSE_BUF_SIZE)
			parse_die("Word too long");
		if (c==' ' || c=='\t' || c=='\n'){
			parse_buf[pos]=0;
			if (c=='\n')
				ungetc(c,f);
			return;
		}
		parse_buf[pos++]=c;
	}
}

bool csv_field(bool eof_ok)
{
	int pos=0;
	char c;
	while (1){
		c=getc(f);
		if (feof(f))
			if (eof_ok)
				return false;
			else
				parse_die("Unexpected EOF");
		if (pos==PARSE_BUF_SIZE)
			parse_die("Word too long");
		if (c==',' || c=='\n'){
			parse_buf[pos]=0;
			if (c=='\n')
				lineno++;
			return true;
		}
		parse_buf[pos++]=c;
	}
}

char *rest_of_line()
{
	int pos=0;
	char c;
	while (1){
		c=getc(f);
		if (feof(f))
			parse_die("Unexpected EOF");
		if (pos==PARSE_BUF_SIZE)
			parse_die("Rest of line too long");
		if (c=='\n'){
			parse_buf[pos]=0;
			lineno++;
			return my_strdup(parse_buf);
		}
		parse_buf[pos++]=c;
	}
}

int parse_id2(char* buf, int* i)
{
	int pos=0;
	char c;
	*i=0;
		
	c=buf[pos];
	do	{
		if (pos>9)
			return TOO_LONG;
		if (c<'0' || c>'9') 
			return GARBAGE;
		*i=(*i)*10+(c-'0');
		pos++;
	} while (c=buf[pos]);

	return OK;
}

int parse_id()
{
	int i;
	int result=parse_id2(parse_buf,&i);
	switch (result){
		case OK:
			return i;
		case TOO_LONG:
			parse_die2("Too many digits");
		case GARBAGE: 
			parse_die2("Garbage in integer");
		default:
			die("bad result from parse_id2()");
	}
}


int parse_amount2(char* buf,int* amount)
{
	int i=0;
	int j=0;
	int pos=0;
	int sign=1;
	int pastdot=false;
	char c;
	if (buf[0]=='-'){
		sign=-1;
		pos++;
	}
		
	c=buf[pos];
	do	{
		if (c=='.') 
			pastdot=true;
		else {
			if (c<'0' || c>'9') 
				return GARBAGE;
			if (pastdot){
				j=j*10+(c-'0');
				if (j>=100)
					return TOO_MANY_DECIMALS;
			} else {  
				i=i*10+(c-'0');
				if (i>=10000000)
					return TOO_LONG;
			}
		}
		pos++;
	} while (c=buf[pos]);

	*amount=(i*100+j)*sign;
	return OK;
}

int parse_amount()
{
	int amount;
	int result=parse_amount2(parse_buf,&amount);
	switch (result){
		case OK:
			return amount;
		case GARBAGE:		
			parse_die2("Garbage in integer");
		case TOO_MANY_DECIMALS:
			parse_die2("Too many decimal places");
		case TOO_LONG:
			parse_die2("Too many digits");
		default:
			die("bad result from parse_amount2()");
	}
}


bool parse_positive()
{
	if (strcmp(parse_buf,"+")==0)
		return true;
	else if (strcmp(parse_buf,"-")==0)
		return false;
	else parse_die2("'+' or '-' expected");
}

char parse_DC()
{
	if (strcmp(parse_buf,"D")==0)
		return 'D';
	else if (strcmp(parse_buf,"C")==0)
		return 'C';
	else parse_die2("'D' or 'C' expected");
}

int parse_date2(char* buf,time_t *time)
{	
	struct tm tt;
	char *result;
	memset (&tt, '\0', sizeof (tt));
	result=strptime (buf, "%d%h%Y", &tt);	
	if (result==NULL || *result!=0)
		return BAD;
	*time=mktime(&tt);
	return OK;
}

time_t parse_date()
{
	time_t time;
	if (parse_date2(parse_buf, &time)!=OK)
		parse_die2("Date unparseable, expected DDMMMYYYY");
	else
		return time;
}

time_t parse_csv_date()
{
	struct tm time;
	char *result;
	memset (&time, '\0', sizeof (time));
	result=strptime (parse_buf, "%d %h %Y", &time);
	if (result==NULL || *result!=0)
		parse_die2("Date unparseable, expected DD MMM YYYY");
	return mktime(&time);
}

void print_date(FILE* out, time_t time)
{ 
	char buf[20];
	strftime (buf, 20, "%d%h%Y", localtime(&time));
	fprintf(out,"%s",buf);
}

void print_amount(FILE* out, int amount)
{
	fprintf(out,"%i.%02i",amount/100,abs(amount)%100);
}	

void print_amount_right(int amount)
{
	printf("%7i.%02i",amount/100,abs(amount)%100);
}

void print_DC(Account* account,int amount)
{
	if (!amount) return;
	bool increase=amount>0;
	bool debit=account->positive?increase:!increase;
	if (!debit)
		printf("        ");
	amount=abs(amount);
	printf("%4i.%02i",amount/100,abs(amount)%100);
	if (debit)
		printf("        ");
}

Account* find_account2(Account *a, int id)
{
	Account* s=NULL;
	while (a){
		if (a->id==id)
			return a;
		s=find_account2(a->sub_accounts,id);
		if (s)
			return s;
		a=a->next;
	}
	return NULL;
}

Account* find_account(int id)
{
	Account* a=find_account2(first_account,id);
	if (!a)
		parse_die2("Undefined account");
	return a;
}

void balanced(int total)
{
	if (total!=0) {
		lineno--;
		parse_die("Transaction doesn't balance");
	}
}

int balance(Transaction* transaction)
{
	int total=0;
	Split* split=transaction->splits;
	while (split) {
		total+=split->account->positive?split->amount:-split->amount;
		split=split->next_transaction_split;
	}
	return total;
}

Account* read_account()
{
	Account* account;
	int depth=0;
	char c;

	while ((c=getc(f))=='\t')
		depth++;
	ungetc(c,f);
	if (feof(f))
		return NULL;

	account=my_malloc(sizeof(Account));
	account->depth=depth;
	next_word();
	account->id=parse_id();
	next_word();
	account->positive=parse_positive();
	account->name=rest_of_line();
	account->first_split=NULL;
	account->last_split=NULL;
	account->balance=0;
	account->next=NULL;
	account->sub_accounts=NULL;
	return account;
}

Account* read_accounts2(Account* last_account)
{
	Account* account=read_account();

	while (account) {
		if (account->depth<last_account->depth) {
			return account;
		}else if (account->depth==last_account->depth){
			last_account->next=account;
			last_account=account;
			account=read_account();
		} else if (account->depth==last_account->depth+1) {
			last_account->sub_accounts=account;
			account=read_accounts2(account);
		} else {
			lineno--;
			parse_die("Tab step by more than 1");
		}
	}
}

void read_accounts()
{
	open_datafile("accounts");
	first_account=read_account();
	read_accounts2(first_account);

	fclose(f);
}


void read_journal()
{
	time_t latest=0;

	open_datafile("journal");
	
	while (!feof(f)){
		Transaction* transaction=my_malloc(sizeof(Transaction));
		Split* current_transaction_split=NULL;
		int total=0;

		// read in transaction line
		next_word();
		transaction->date=parse_date();
		
		//	parse_die2("Time is going backwards");
		
		transaction->desc=rest_of_line();
		transaction->splits=NULL;
			
		//print_date(stdout,transaction->date);
		if (last_transaction) {
			if (transaction->date<first_transaction->date){
				transaction->next=first_transaction;
				first_transaction=transaction;
				//printf("front\n");
			} else if (transaction->date<latest) {
				Transaction* tt=first_transaction;
				while (tt->next->date<=transaction->date)
					tt=tt->next;
				transaction->next=tt->next;
				tt->next=transaction;
				//printf("middle ");
				//print_date(stdout,tt->date);
				//printf(" ");
				//print_date(stdout,transaction->next->date);
				//printf("\n");
			} else {
				last_transaction->next=transaction;
				last_transaction=transaction;
				latest=transaction->date;
				//printf("end\n");
			}
		} else { 
			first_transaction=last_transaction=transaction;
			latest=transaction->date;
			//printf("first\n");
		}

		// read in splits
		while (1){
			char c;
			Split* split;

			c=getc(f);
			
			// handle eof
			if (feof(f)){
				balanced(total);
				fclose(f);
				return;
			}
			// handle new transaction 
			if (c!='\t'){
				balanced(total);
				ungetc(c,f);
				break;
			}

			split=my_malloc(sizeof(Split));

			// read in split line
			next_word();
			split->transaction=transaction;
			split->account=find_account(parse_id());
			next_word();
			split->amount=parse_amount();
			split->desc=rest_of_line();
			split->next_transaction_split=NULL;
			split->next_account_split=NULL;

			// append to transaction splits
			total+=split->account->positive?split->amount:-split->amount;
			if (current_transaction_split) {
				current_transaction_split->next_transaction_split=split;
				current_transaction_split=split;
			} else
				transaction->splits=current_transaction_split=split;
			
			// append to account splits
			split->account->balance+=split->amount;
			if (split->account->first_split) {
				if (transaction->date<split->account->first_split->transaction->date){
					split->next_account_split=split->account->first_split;
					split->account->first_split=split;	
					//printf("front\n");
				} else if (transaction->date<split->account->last_split->transaction->date) {
					Split* ss=split->account->first_split;
					while (ss->next_account_split->transaction->date<=transaction->date)
						ss=ss->next_account_split;
					split->next_account_split=ss->next_account_split;
					ss->next_account_split=split;
					//printf("middle ");
					//print_date(stdout,tt->date);
					//printf(" ");
					//print_date(stdout,transaction->next->date);
					//printf("\n");
				} else {
					split->account->last_split->next_account_split=split;
					split->account->last_split=split;
				}
			} else { 
				split->account->first_split=split;
				split->account->last_split=split;
			}

		}
	}
} 

void verify(){
	read_accounts();
	read_journal();
}

void print_transaction(FILE* out, Transaction* t)
{
	Split *s;
	print_date(out,t->date);
	fprintf(out," %s\n",t->desc);
	s=t->splits;
	while (s) {
		fprintf(out,"\t%i ",s->account->id);
		print_amount(out,s->amount);
		if (s->desc)
			fprintf(out," %s",s->desc);
		fprintf(out,"\n");
		s=s->next_transaction_split;
	}
}

void print_journal()
{
	verify(); 

	Transaction* t=first_transaction;	
	while (t){
		print_transaction(stdout,t);	
		t=t->next;
	}	
}


void print_accounts2(Account* a, int indent)
{
	while (a){
		int t;
		for (t=0;t<indent;t++)
			printf("\t");
		printf("%i %c %s\n",a->id,a->positive?'+':'-',a->name);
		if (a->sub_accounts)
			print_accounts2(a->sub_accounts,indent+1);
		a=a->next;
	}
}

void print_accounts()
{
	read_accounts();
	print_accounts2(first_account,0);
}

Transaction* new_transaction(time_t date,char* desc)
{
	Transaction* transaction=my_malloc(sizeof(Transaction));
	transaction->date=date;
	transaction->desc=desc;
	transaction->splits=NULL;
	transaction->next=NULL;
}

void add_split(Transaction* t,Split* s)
{
	if (!t->splits)
		t->splits=s;
	else {
		Split* ss=t->splits;
		while (ss->next_transaction_split)
			ss=ss->next_transaction_split;
		ss->next_transaction_split=s;
	}
}

Split* new_split(Account* account, int amount, char* desc)
{
	Split* split=my_malloc(sizeof(Split));
	split->amount=amount;
	split->account=account;
	split->desc=desc;
	split->next_transaction_split=NULL;
	split->next_account_split=NULL;
	return split;
}


void clear()
{
	printf("\x1b[H\x1b[2J");
}

void print_str(char* str, int len)
{
	int str_len=strlen(str);
	
	if (str_len<len)
		printf("%-*.*s ",len,len,str);
	else
		printf("%-*.*s... ",len-3,len-3,str);
}

void print_transaction_splits(Split* split)
{
	if (!split)
		return;
	printf("    %5i ",split->account->id);
	print_str(split->account->name,15);
	print_str(split->desc,27);
	print_DC(split->account,split->amount);
	printf("\n");
	print_transaction_splits(split->next_transaction_split);
}

void import_display(Account* account,time_t csv_date,int csv_amount,char* csv_desc,Transaction* transaction)
{
	Split* split;
	clear();
	printf("Importing to: %s\n\n",account->name);
	
	print_date(stdout,csv_date);
	printf("  $");	
	print_amount(stdout,csv_amount);
	printf("\n%s %s\n\n",csv_desc,csv_ref);
	//printf("%s\n\n",csv_desc);
	
	print_date(stdout,transaction->date);
	printf(" %s\n",transaction->desc);

	print_transaction_splits(transaction->splits);

	printf("\n");
}

Account *input_account()
{
	int result;
	int account_id;
	Account* account;
	char* account_str;
	do {
		do {
			account_str=readline("Account: ");
			result=parse_id2(account_str,&account_id);
			free(account_str);
		} while (result);
		account=find_account2(first_account,account_id);
	} while (!account);
	return account;
}


bool finish_off_transaction(Transaction* transaction)
{
	int result;
	int rest=balance(transaction);
	
	if (transaction->desc[0]==0)
		transaction->desc=readline("Description: ");
	add_history(transaction->desc);
	
	while (rest){
		Account* account;
		char* amount_str;
		int amount;
		
		import_display(csv_account,csv_date,csv_amount,csv_desc,transaction);
		printf("Remaining $");
		print_amount(stdout,abs(rest));
		printf("\n");
		
		account=input_account();
			
		do {		
			amount_str=readline("Amount: ");
			if (amount_str[0]==0){
				amount=account->positive?-rest:rest;
				result=OK;
			} else 
				result=parse_amount2(amount_str,&amount);
			//printf("%i %i %s %i\n",rest,result,amount_str,amount);
			free(amount_str);
		} while (result);

		add_split(transaction,new_split(account,amount,""));
		rest=balance(transaction);
	}

	import_display(csv_account,csv_date,csv_amount,csv_desc,transaction);
	printf("[Commit]/Redo: ");
	getchar();
	result=getchar();
	if (result=='r' || result=='R')
		return true;
	else {
		print_transaction(journal,transaction);
		return false;
	} 
}

int gst(int amount)
{
	//extract the gst component
	//add 4 so to get correct rounding
	if (amount>0)
		amount+=4;
	else
		amount-=4;
	return amount/9;
}

char *compose(char* a, char*b)
{
	int aa=strlen(a);
	int bb=strlen(b)+1;
	char* c=my_malloc(aa+bb);
	strncpy(c,a,aa);
	strncpy(c+aa,b,bb);
	return c;
}

void import()
{
	int i;
	bool retry;
	//Account* gst_collected;
	//Account* gst_paid;
	//Account* drawings;
	Account* interest;
	//Account* withholding_tax;
	//Account* bank_fees;
	Account* drawings_rich;
	Account* drawings_steph;
	int rich_amount, steph_amount;
	Transaction* transaction;
	
	if (argc!=4) {
		help();
		exit(1);
	}

	if (strlen(argv[1])>10)
		die("Account number too long");
	read_accounts();
	strcpy(parse_buf,argv[2]);
	csv_account=find_account(parse_id());
	//gst_collected=find_account(211);
	//gst_paid=find_account(212);
	//drawings=find_account(32);
	interest=find_account(431);
	//withholding_tax=find_account(332);
	//bank_fees=find_account(407);
	drawings_rich=find_account(311);
	drawings_steph=find_account(312);

	open_datafile(argv[3]);

	journal=fopen("journal","a");
	if (!journal)
		die("Could not open 'journal' for appending");


	rl_bind_key ('\t', rl_insert);//turn off readline file completion

	csv_field(false);
	csv_field(false);
	csv_field(false);
	csv_field(false);
	csv_field(false);
	while (csv_field(true)){
		printf("parse_buf=%s\n",parse_buf);
		csv_date=parse_csv_date();
		csv_field(false);
		strcpy(csv_desc,parse_buf);
		csv_field(false);
		strcpy(csv_ref,parse_buf);
		csv_field(false);
		csv_amount=parse_amount();
		csv_field(false);

		if (!csv_account->positive)
			csv_amount=-csv_amount;

		do {
			transaction=new_transaction(csv_date,""); //this leaks
			retry=false;
			import_display(csv_account,csv_date,csv_amount,csv_desc,transaction);
			//printf("Expense/Income/Drawings/iNterest/Transfer/Withholdingtax/Bankfees/Free/sKip: ");
			printf("Transfer/Free/sKip: ");
			char type=getchar();
			switch (type){
/*
				case 'E':
				case 'e':
					add_split(transaction,new_split(csv_account,csv_amount,""));
					add_split(transaction,new_split(gst_paid,gst(csv_amount),""));
					retry=finish_off_transaction(transaction);
					break;
				case 'N':
				case 'n':
					transaction->desc="Interest";
					add_split(transaction,new_split(csv_account,csv_amount,""));
					add_split(transaction,new_split(interest,csv_amount,""));
					retry=finish_off_transaction(transaction);
					break;
*/
				case 'T':
				case 't':
					{
					Account* transfer_account=input_account();
					int transfer_dir=csv_amount<0;
					transaction->desc="Transfer";
					add_split(transaction,new_split(csv_account,csv_amount,compose(transfer_dir?"To ":"From ",transfer_account->name)));
					add_split(transaction,new_split(transfer_account,-csv_amount,compose(transfer_dir?"From ":"To ",csv_account->name)));
					retry=finish_off_transaction(transaction);
					break;
					}
/*
				case 'W':
				case 'w':
					transaction->desc="Witholding Tax on Interest";
					add_split(transaction,new_split(csv_account,csv_amount,""));
					add_split(transaction,new_split(withholding_tax,csv_amount,""));
					retry=finish_off_transaction(transaction);
					break;
				case 'B':
				case 'b':
					transaction->desc="Bank Fees";
					add_split(transaction,new_split(csv_account,csv_amount,""));
					add_split(transaction,new_split(bank_fees,-csv_amount,""));
					retry=finish_off_transaction(transaction);
					break;
*/
				case 'F':
				case 'f':
					add_split(transaction,new_split(csv_account,csv_amount,""));
					retry=finish_off_transaction(transaction);
					break;
				case 'K':
				case 'k':
					retry=false;
					break;
				case 'D':
				case 'd':
					rich_amount = csv_amount/2;
					steph_amount = csv_amount - rich_amount;
					transaction->desc="Drawings";
					add_split(transaction,new_split(csv_account,csv_amount,""));
					add_split(transaction,new_split(drawings_rich,-rich_amount,""));
					add_split(transaction,new_split(drawings_steph,-steph_amount,""));
					retry=finish_off_transaction(transaction);
					break;
				case 'I':
				case 'i':
					transaction->desc="Interest";
					add_split(transaction,new_split(csv_account,csv_amount,""));
					add_split(transaction,new_split(interest,csv_amount,""));
					retry=finish_off_transaction(transaction);
					break;
				case 'Q':
				case 'q':
					fclose(f);
					fclose(journal);
					exit(0);
				default:
					retry=true;
			}
		} while (retry);
	}
	fclose(f);
	fclose(journal);
}


int sub_accounts_total2(Account* a)
{
	if (!a) return 0;
	int total=0;
	while (a) {
		total+=a->balance;
		total+=sub_accounts_total2(a->sub_accounts);
		a=a->next;
	}
	return total;
}

int sub_accounts_total(Account* a)
{
	return a->balance+sub_accounts_total2(a->sub_accounts);
}

void summary2(Account* a, int indent)
{
	while (a){
		int t;
		int l;
		for (t=0;t<indent;t++)
			putchar(' ');
		printf("%i %-20s%n",a->id,a->name,&l);
		for (t=indent+l;t<30;t++)
			putchar(' ');
		if (a->balance || !a->sub_accounts)
			print_amount_right(a->balance);
		else {
			printf("          ");
			print_amount_right(sub_accounts_total(a));
		}			
		putchar('\n');
		if (a->sub_accounts)
			summary2(a->sub_accounts,indent+2);
		a=a->next;
	}
}

void summary()
{
	read_accounts();
	read_journal();
	printf("Account Summary\n");
	printf("===============\n");
	summary2(first_account,0);
}


void statement()
{
	int acc_id,i,n,balance;
	Account* account;
	Split* split;
	bool show_splits;

	if (argc!=3) {
		help();
		exit(1);
	}
	
	show_splits=strcmp("statement",argv[1]);

	if (parse_id2(argv[2], &acc_id)!=OK)
		die("Bad account number");

	read_accounts();
	account=find_account(acc_id);
	read_journal();

	printf("Statement of Account '%s' (%i)%n\n",account->name,account->id,&n);
	for (i=0;i<n;i++)
		putchar('=');
	putchar('\n');

	split=account->first_split;
	balance=0;
	while(split){
		balance+=split->amount;
		print_date(stdout,split->transaction->date);
		putchar(' ');
		print_str(!show_splits && split->desc[0]?split->desc:split->transaction->desc,43);
		print_DC(account,split->amount);
		print_amount_right(balance);
		putchar('\n');
		if (show_splits)
			print_transaction_splits(split->transaction->splits);
		split=split->next_account_split;
	}

}

char* months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};

void kiwibank_date(struct tm* date)
{
	printf("%i %s %i",date->tm_mday,months[date->tm_mon],date->tm_year+1900);
}

void download_dates()
{
	int oneday=24*60*60;
	time_t from_date;
	time_t yesterday=time(NULL)-oneday;
	struct tm from;
	struct tm to;
	int acc_id;
	Account* account;

	if (argc!=3) {
		help();
		exit(1);
	}
	
	if (parse_id2(argv[2], &acc_id)!=OK)
		die("Bad account number");

	read_accounts();
	account=find_account(acc_id);
	read_journal();

	from_date=account->last_split->transaction->date+oneday;

	localtime_r(&from_date,&from);
	localtime_r(&yesterday,&to);

	printf("STARTDATE=");
	kiwibank_date(&from);
	printf("&ENDDATE=");	
	kiwibank_date(&to);
}

void invoice()
{
	int cust_id;
	int amount;
	time_t date;
	Account* inc_acc;
	Account* acc_rec;

	if (argc!=6) {
		help();
		exit(1);
	}
	
	read_accounts();

	// invoice <cust#> <date> <desc> <amount>

	if (parse_id2(argv[2], &cust_id)!=OK)
		die("Bad customer number");

	if (parse_date2(argv[3], &date)!=OK)
		die("Bad date, expecting ddmmmyyyy");

	if (parse_amount2(argv[5], &amount)!=OK)
		die("Bad amount");
	
	inc_acc=find_account2(first_account,cust_id+510);
	if (!inc_acc)
		die("No income account");

	acc_rec=find_account2(first_account,cust_id+130);
	if (!inc_acc)
		die("No account recievable");

	Transaction* transaction=new_transaction(date,argv[4]);
	add_split(transaction,new_split(inc_acc,amount,""));
	add_split(transaction,new_split(acc_rec,amount,""));

	journal=fopen("journal","a");
	if (!journal)
		die("Could not open 'journal' for appending");
	print_transaction(journal,transaction);
	fclose(journal);
}

void acc_balance()
{
	int acc_id;
	Account* account;

	if (argc!=3) {
		help();
		exit(1);
	}

	if (parse_id2(argv[2], &acc_id)!=OK)
		die("Bad account number");

	read_accounts();
	read_journal();

	account=find_account(acc_id);
	print_amount(stdout,account->balance);
}


/******** MAIN ********/

Command commands[] = {
	{ "accounts", print_accounts },
	{ "journal", print_journal },
	{ "verify", verify },
	{ "import", import },
	{ "summary", summary },
	{ "statement", statement },
	{ "statement-splits", statement },
	{ "download-dates", download_dates },
	{ "invoice", invoice },
	{ "balance", acc_balance },
	{ NULL , NULL }
};

int main(int main_argc, char **main_argv) {
	Command* command=commands;

	argc=main_argc;
	argv=main_argv;

	if (argc<2) {
		help();
		exit(0);
	}
	while (command->text){
		if (strcmp(command->text,argv[1])==0){
			command->fun();
			exit(0);
		}
		command++;
	}
	help();
	exit(0);
}
