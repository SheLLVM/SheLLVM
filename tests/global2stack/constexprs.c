typedef struct _FooStruct {
	int a;
	int b;
	int c;
	int d;
} FooStruct;

char foo[1024];

__attribute__((annotate("shellvm-main")))
int main()
{
	FooStruct *fs = (FooStruct *)&foo[256];

	fs->a = 1;
	fs->b = 2;
	fs->c = 3;
	fs->d = 4;

	return 0;
}
