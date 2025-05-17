#include <iostream>
#define MEMPOOL_ALIGNMENT 8 // 对齐长度
using namespace std;

#include "memorypool.h"
class user
{
    int s;
    double s1;
    double s3;

public:
    user(int x) : s(x)
    {
        cout << "-------------------调用 user 的构造函数----------------" << endl;
    }
    int get()
    {
        return s;
    }
    ~user()
    {
        cout << "------------------调用user 的析构函数------------------" << endl;
    }
};

int main()
{
    memorypool<user> m_pool;
    user *dp1 = (user *)m_pool.allocate(1);
    cout << "dp1= " << dp1 << endl;
    new (dp1) user(1111);
    cout << " 对象中的数据值为 " << dp1->get() << endl;
    user *dp2 = (user *)m_pool.allocate(1);
    cout << "dp2= " << dp2 << endl;
    new (dp2) user(2222);
    cout << "对象中的数据值为 " << dp2->get() << endl;
    user *dp3 = (user *)m_pool.allocate(1);
    cout << "dp3= " << dp3 << endl;
    new (dp3) user(3333);
    cout << "对象中的数据值为 " << dp3->get() << endl;
    user *dp4 = (user *)m_pool.allocate(1);
    cout << "dp4= " << dp4 << endl;
    new (dp4) user(4444);
    cout << "对象中的数据值为 " << dp4->get() << endl;
    user *dp5 = (user *)m_pool.allocate(1);
    cout << "dp5= " << dp5 << endl;
    new (dp5) user(5555);
    cout << "对象中的数据值为 " << dp5->get() << endl;
    user *dp6 = (user *)m_pool.allocate(1);
    cout << "dp6= " << dp6 << endl;
    new (dp6) user(6666);
    cout << " 对象中的数据值为 " << dp6->get() << endl;

    user *dp7 = (user *)m_pool.allocate(1);
    cout << "dp7= " << dp7 << endl;
    new (dp7) user(7777);
    cout << " 对象中的数据值为 " << dp7->get() << endl;

    user *dp8 = (user *)m_pool.allocate(1);
    cout << "dp8= " << dp8 << endl;
    new (dp8) user(8888);
    cout << " 对象中的数据值为 " << dp8->get() << endl;

    user *dp9 = (user *)m_pool.allocate(1);
    cout << "dp9= " << dp9 << endl;
    new (dp9) user(9999);
    cout << " 对象中的数据值为 " << dp9->get() << endl;

    user *dp10 = (user *)m_pool.allocate(1);
    cout << "dp10= " << dp10 << endl;
    new (dp10) user(11111);
    cout << " 对象中的数据值为 " << dp10->get() << endl;

    user *dp11 = (user *)m_pool.allocate(1);
    cout << "dp11= " << dp11 << endl;
    new (dp11) user(22222);
    cout << " 对象中的数据值为 " << dp11->get() << endl;

    user *dp12 = (user *)m_pool.allocate(1);
    cout << "dp12= " << dp12 << endl;
    new (dp12) user(33333);
    cout << " 对象中的数据值为 " << dp12->get() << endl;

    user *dp13 = (user *)m_pool.allocate(1);
    cout << "dp13= " << dp13 << endl;
    new (dp13) user(44444);
    cout << " 对象中的数据值为 " << dp13->get() << endl;

    user *dp14 = (user *)m_pool.allocate(1);
    cout << "dp14= " << dp14 << endl;
    new (dp14) user(55555);
    cout << " 对象中的数据值为 " << dp14->get() << endl;

    user *dp15 = (user *)m_pool.allocate(1);
    cout << "dp15= " << dp15 << endl;
    new (dp15) user(66666);
    cout << " 对象中的数据值为 " << dp15->get() << endl;

    m_pool.print_status();

    m_pool.free(dp1);
    m_pool.free(dp2);
    m_pool.free(dp3);
    m_pool.print_status();

    m_pool.free(dp4);
    m_pool.free(dp5);
    m_pool.free(dp6);
    m_pool.free(dp7);

    m_pool.print_status();
    m_pool.free(dp8);
    m_pool.free(dp9);
    m_pool.free(dp10);
    m_pool.free(dp11);
    m_pool.free(dp12);
    m_pool.free(dp13);
    m_pool.free(dp14);
    m_pool.free(dp15);
    m_pool.print_status();
    return 0;
}