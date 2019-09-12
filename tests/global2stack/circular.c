struct Something {
	int x;
	int y;
	struct Something *next;
	int z;
};

extern struct Something b;
struct Something a = { 100, 10, &b, 1 };
struct Something b = { 200, 20, &a, 2 };

__attribute__((annotate("shellvm-main")))
int main()
{
	struct Something d = { 300, 30, &b, 3 };
	struct Something c = { 400, 40, &d, 4 };
	return a.next->next != c.next->next->next;
}
