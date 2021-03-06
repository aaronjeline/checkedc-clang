// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

typedef unsigned long size_t;
#define NULL ((void*)0)
typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

void f() {
  int x[5];
  int *pa = x;
  pa += 2;
  int *p = pa;
  p = (int *)5;
}
// CHECK: int x[5];

void g() {
  int *x = malloc(sizeof(int)*1);
  // CHECK_ALL:  _Array_ptr<int> x : count(1) =  malloc<int>(sizeof(int)*1);
  // CHECK_NOALL: int *x = malloc<int>(sizeof(int)*1);
  int y[5];
  // CHECK_ALL:  int y _Checked[5];
  // CHECK_NOALL: int y[5];
  int **p = &x;
  // CHECK_ALL:  _Ptr<_Array_ptr<int>> p =  &x;
  // CHECK_NOALL: _Ptr<int *> p =  &x;
  int **r = 0;
  // CHECK_ALL:  _Ptr<_Array_ptr<int>> r =  0;
  // CHECK_NOALL: _Ptr<int *> r =  0;
  *p = y;
  (*p)[0] = 1;
  r = p;
  **r = 1;
}





void foo(void) {
  int x;
  int *y = &x;
  int **z = &y;
  // CHECK:  _Ptr<int> y =  &x;
  // CHECK:  _Ptr<_Ptr<int>> z =  &y;

  int *p = &x;
  int **q = &p;
  q = (int **)5;

  int *p2 = &x;
  p2 = (int *)5;
  int **q2 = &p2;
}
// CHECK:  _Ptr<int *> q2 = &p2;
