int main (void)
{
	int op;
	int dummy;


	op = 1;
	dummy = 7;
	while(1)
	{
			if(op)
			{
				dummy *= 44*22*33;
				op = 0;
			}
			else
			{
				dummy /= 44*22*33;
				op = 1;
			}
	}

	return 0;
}
