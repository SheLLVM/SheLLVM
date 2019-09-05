void a()
{
	volatile static int x = 0;
	x++;
	return;
}

void b()
{
	a();
	return;
}

void c(int arg)
{
	if(arg)
		b();
	a();
	return;
}

void d()
{
	b();
	return;
}

__attribute__((annotate("shellvm-main")))
int main(int argc, char *argv[])
{
	if(argc&1)
		d();
	c(argc&2);
	return 0;
}
