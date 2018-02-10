//
// Created by xzl on 3/17/17.
//

/* a pre allocated mem allocator
 *
 * http://stackoverflow.com/questions/14807192/can-i-use-an-stdvector-as-a-facade-for-a-pre-allocated-raw-array
 *
 *
 * g++-5 -std=c++11 alloc-pre.cpp -o alloc-pre.bin
 *
 * */

#include <memory>
#include <iostream>
#include <vector>

using namespace std;

template <typename T>
class placement_memory_allocator: public std::allocator<T>
{
	void* pre_allocated_memory;
public:
	typedef size_t size_type;
	typedef T* pointer;
	typedef const T* const_pointer;

	template<typename _Tp1>
	struct rebind
	{
		typedef placement_memory_allocator<_Tp1> other;
	};

	pointer allocate(size_type n, const void *hint=0)
	{
		char* p = new(pre_allocated_memory)char[n * sizeof(T)];
		cout << "Alloc " << dec << n * sizeof(T) << " bytes @" << hex << (void*)p <<endl;
		return (T*)p;
	}

	void deallocate(pointer p, size_type n)
	{
		cout << "Dealloc " << dec << n * sizeof(T) << " bytes @" << hex << p << endl;
		//delete p;
	}

	placement_memory_allocator(void* p = 0) throw(): std::allocator<T>(), pre_allocated_memory(p) { cout << "Hello allocator!" << endl; }
	placement_memory_allocator(const placement_memory_allocator &a) throw(): std::allocator<T>(a) {pre_allocated_memory = a.pre_allocated_memory;}
	~placement_memory_allocator() throw() { }
};

class MyClass
{
	char empty[10];
	char* name;
public:
	MyClass(char* n) : name(n){ cout << "MyClass: " << name << " @" << hex << (void*)this << endl; }
	MyClass(const MyClass& s){ name = s.name; cout << "=MyClass: " << s.name << " @" << hex << (void*)this << endl; }
	~MyClass(){ cout << "~MyClass: " << name << " @" << hex << (void*)this <<  endl; }
};

#define DIRECT_X_MEMORY_SIZE_IN_BYTES 4096

int main()
{
	void * buf = malloc(DIRECT_X_MEMORY_SIZE_IN_BYTES);
	// create allocator object, intialized with DirectX memory ptr.
	placement_memory_allocator<MyClass> pl(buf);
	//Create vector object, which use the created allocator object.
	vector<MyClass, placement_memory_allocator<MyClass>> v(pl);

	cout << "xzl: vector init done " << endl;
	// !important! reserve all the memory of directx buffer.
	// try to comment this line and rerun to see the difference
	v.reserve( DIRECT_X_MEMORY_SIZE_IN_BYTES / sizeof(MyClass));

	cout << "xzl: reserv done" << endl;

	//some push backs.
	v.push_back(MyClass("first"));
	cout << "Done1" << endl;
	v.push_back(MyClass("second"));
	cout << "Done1" << endl;
	v.push_back(MyClass("third"));
	cout << "Done1" << endl;

// resize, see if the allocator free some space?
	cout <<  "before shink to fit" << endl;
	v.shrink_to_fit();   // this may trigger reallocation.
	cout <<  "shink to fit done" << endl;

	cout << "v has " << v.size() << " elements" << endl;
}
