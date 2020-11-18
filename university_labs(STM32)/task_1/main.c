int factorial(int x); //функция нахождения факториала

int main(void)
{
    volatile int Y=0;
    volatile int X=6;
    volatile int x=0;
    Y=factorial(X); 
    return 0;
}

int factorial(int x) //передача значения 6 в функцию
{
	volatile int y=1; // исходный множитель
	for(volatile int i=1; i<x; i++) //цикл для нахождения факториала
	{
		y=y*(i+1); 
	}
	return y; //возвращение значения факториала
}