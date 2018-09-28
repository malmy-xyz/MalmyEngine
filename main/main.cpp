#include "main.h"


int main(int argc, char * argv[])
{
	std::cout << "Hello World!" << std::endl;

	TestClass::TestClass();
	
	TestClass* t = new TestClass();

	t->TestFunction();
	t->m_varPublic = 3;
	t->TestFunction();


	system("pause");

}

