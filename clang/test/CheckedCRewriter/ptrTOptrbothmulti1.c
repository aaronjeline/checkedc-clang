// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checkedALL %s %S/ptrTOptrbothmulti2.c
// RUN: cconv-standalone -base-dir=%S -output-postfix=checkedNOALL %s %S/ptrTOptrbothmulti2.c
//RUN: %clang -c %S/ptrTOptrbothmulti1.checkedNOALL.c %S/ptrTOptrbothmulti2.checkedNOALL.c
//RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %S/ptrTOptrbothmulti1.checkedNOALL.c %s
//RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %S/ptrTOptrbothmulti1.checkedALL.c %s
//RUN: rm %S/ptrTOptrbothmulti1.checkedALL.c %S/ptrTOptrbothmulti2.checkedALL.c
//RUN: rm %S/ptrTOptrbothmulti1.checkedNOALL.c %S/ptrTOptrbothmulti2.checkedNOALL.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: having a pointer to a pointer*/
/*For robustness, this test is identical to ptrTOptrprotoboth.c and ptrTOptrboth.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo will treat its return value safely, but sus and bar will not,
through invalid pointer arithmetic, an unsafe cast, etc.*/

/*********************************************************************************/


typedef unsigned long size_t;
#define NULL 0
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

struct general { 
    int data; 
    struct general *next;
	//CHECK: _Ptr<struct general> next;
};

struct warr { 
    int data1[5];
	//CHECK_NOALL: int data1[5];
	//CHECK_ALL: int data1 _Checked[5];
    char *name;
	//CHECK: _Ptr<char> name;
};

struct fptrarr { 
    int *values; 
	//CHECK: _Ptr<int> values; 
    char *name;
	//CHECK: _Ptr<char> name;
    int (*mapper)(int);
	//CHECK: _Ptr<int (int )> mapper;
};

struct fptr { 
    int *value; 
	//CHECK: _Ptr<int> value; 
    int (*func)(int);
	//CHECK: _Ptr<int (int )> func;
};  

struct arrfptr { 
    int args[5]; 
	//CHECK_NOALL: int args[5]; 
	//CHECK_ALL: int args _Checked[5]; 
    int (*funcs[5]) (int);
	//CHECK_NOALL: int (*funcs[5]) (int);
	//CHECK_ALL: _Ptr<int (int )> funcs _Checked[5];
};

int add1(int x) { 
    return x+1;
} 

int sub1(int x) { 
    return x-1; 
} 

int fact(int n) { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

int fib(int n) { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

int zerohuh(int n) { 
    return !n;
}

int *mul2(int *x) { 
	//CHECK: _Ptr<int> mul2(_Ptr<int> x) { 
    *x *= 2; 
    return x;
}

char *** sus(char * * *, char * * *);
	//CHECK_NOALL: char *** sus(char ***, _Ptr<_Ptr<_Ptr<char>>> y);
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> sus(char ***, _Ptr<_Ptr<_Ptr<char>>> y);

char *** foo() {
	//CHECK_NOALL: char *** foo(void) {
	//CHECK_ALL: _Ptr<_Array_ptr<char *>> foo(void) {
        char * * * x = malloc(sizeof(char * *));
	//CHECK: char * * * x = malloc<char **>(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
	//CHECK: _Ptr<_Ptr<_Ptr<char>>> y =  malloc<_Ptr<_Ptr<char>>>(sizeof(char * *));
        char *** z = sus(x, y);
	//CHECK_NOALL: char *** z = sus(x, y);
	//CHECK_ALL: _Ptr<_Array_ptr<char *>> z =  sus(x, y);
return z; }

char *** bar() {
	//CHECK_NOALL: char *** bar(void) {
	//CHECK_ALL: _Ptr<_Array_ptr<char *>> bar(void) {
        char * * * x = malloc(sizeof(char * *));
	//CHECK: char * * * x = malloc<char **>(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
	//CHECK: _Ptr<_Ptr<_Ptr<char>>> y =  malloc<_Ptr<_Ptr<char>>>(sizeof(char * *));
        char *** z = sus(x, y);
	//CHECK_NOALL: char *** z = sus(x, y);
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> z =  sus(x, y);
z += 2;
return z; }
