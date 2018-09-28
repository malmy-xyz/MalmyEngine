#include "testClass.h"


TestClass::TestClass()
{
	std::cout << "Constructor TestClass" << std::endl;

	m_varPublic = 0;
	m_varPrivate = 0;

}

void TestClass::TestFunction() 
{
	std::cout << "TestFunction TestClass" << std::endl;
	std::cout << m_varPublic << " " << m_varPrivate << std::endl;
}

TestClass::~TestClass()
{
	std::cout << "Destructor TestClass" << std::endl;
}
