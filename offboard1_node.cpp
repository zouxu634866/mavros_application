#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>  //发布的消息体对应的头文件，该消息体的类型为geometry_msgs：：PoseStamped
#include <mavros_msgs/CommandBool.h>  //CommandBool服务的头文件，该服务的类型为mavros_msgs：：CommandBool
#include <mavros_msgs/SetMode.h>     //SetMode服务的头文件，该服务的类型为mavros_msgs：：SetMode
#include <mavros_msgs/State.h>  //订阅的消息体的头文件，该消息体的类型为mavros_msgs：：State

using namespace std;

//*******由于在本程序中需要订阅两个信息，一个是状态，还有一个是本地位置，所以需要书写两个回调函数********//

//建立一个订阅消息体类型的变量，用于存储订阅的状态信息
mavros_msgs::State current_state;
//订阅时的回调函数，接受到该消息体的内容时执行里面的内容，这里面的内容就是赋值
void state_cb(const mavros_msgs::State::ConstPtr& msg)
{
    current_state = *msg;
}
//建立一个订阅消息体类型的变量，用于存储本地位置信息
geometry_msgs::PoseStamped local_pos;
//订阅时的回调函数，接受到该消息体的内容时执行里面的内容，这里面的内容就是赋值
void local_position_cb(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
    local_pos = *msg;
}

//***************************************以上是回调函数*************************************//



int main(int argc,char **argv)
{
     ros::init(argc,argv,"offborad1");
     ros::NodeHandle nh;

     //启动订阅、发布、服务
     //订阅。<>里面为模板参数，传入的是订阅的消息体类型，（）里面传入三个参数，分别是该消息体的位置、缓存大小（通常为1000）、回调函数
     ros::Subscriber state_sub = nh.subscribe<mavros_msgs::State>("mavros/state", 10, state_cb);
     ros::Subscriber local_pos_sub = nh.subscribe<geometry_msgs::PoseStamped>("mavros/setpoint_position/local", 10, local_position_cb);

     //发布之前需要公告，并获取句柄，发布的消息体的类型为：geometry_msgs::PoseStamped
     ros::Publisher local_pos_pub = nh.advertise<geometry_msgs::PoseStamped>("mavros/setpoint_position/local", 10);

     //启动服务1，设置客户端（Client）名称为arming_client，客户端的类型为ros::ServiceClient，
     //启动服务用的函数为nh下的serviceClient<>()函数，<>里面是该服务的类型，（）里面是该服务的名称
     ros::ServiceClient arming_client = nh.serviceClient<mavros_msgs::CommandBool>("mavros/cmd/arming");

     //启动服务2，设置客户端（Client）名称为set_mode_client，客户端的类型为ros::ServiceClient，
     //启动服务用的函数为nh下的serviceClient<>()函数，<>里面是该服务的类型，（）里面是该服务的名称
     ros::ServiceClient set_mode_client = nh.serviceClient<mavros_msgs::SetMode>("mavros/set_mode");

     ros::Rate rate(20.0);

     // 等待飞控连接mavros，current_state是我们订阅的mavros的状态，连接成功在跳出循环
     while(ros::ok() && !current_state.connected){
         ros::spinOnce();
         rate.sleep();
     }

     while(ros::ok())  //安全起见，只有遥控器切换到acro模式时才执行下面的代码
     {
         if(current_state.mode == "ACRO") break;
         ros::spinOnce(); //只要有while(ros::ok())这个循环，这两句话就要加在里面
         rate.sleep();
     }


     //先实例化一个geometry_msgs::PoseStamped类型的对象，并对其赋值，最后将其发布出去
     geometry_msgs::PoseStamped pose;
     pose.pose.position.x = 0;
     pose.pose.position.y = 0;
     pose.pose.position.z = 10;

     //建立一个类型为SetMode的服务端offb_set_mode，并将其中的模式mode设为"OFFBOARD"，作用便是用于后面的
     //客户端与服务端之间的通信（服务）
     mavros_msgs::SetMode offb_set_mode;
     offb_set_mode.request.custom_mode = "OFFBOARD";

     //建立一个类型为SetMode的服务端offb_set_mode2，并将其中的模式mode设为"AUTO.LAND"，作用便是用于后面的
     //客户端与服务端之间的通信（服务）
     mavros_msgs::SetMode offb_set_mode2;
     offb_set_mode2.request.custom_mode = "AUTO.LAND";

     //建立一个类型为CommandBool的服务端arm_cmd，并将其中的是否解锁设为"true"，作用便是用于后面的
     //客户端与服务端之间的通信（服务）
     mavros_msgs::CommandBool arm_cmd;
     arm_cmd.request.value = true;

     //更新时间
     ros::Time last_request = ros::Time::now();


     int step=0; //用于选择要进行第几步骤
     int waiting_time=0;//用于实现飞机在每一个点的悬停等待

     while(ros::ok())//进入大循环
     {
         //首先判断当前模式是否为offboard模式，如果不是，则客户端set_mode_client向服务端offb_set_mode发起请求call，
         //然后服务端回应response将模式返回，这就打开了offboard模式
         if( current_state.mode != "OFFBOARD" && (ros::Time::now() - last_request > ros::Duration(5.0)))
         {
             if( set_mode_client.call(offb_set_mode) && offb_set_mode.response.mode_sent)
             {
                 ROS_INFO("Offboard enabled");//打开模式后打印信息
             }
             last_request = ros::Time::now();
         }
         else //else指已经为offboard模式，然后进去判断是否解锁，如果没有解锁，则客户端arming_client向服务端arm_cmd发起请求call然后服务端回应response成功解锁，这就解锁了
         {
             if( !current_state.armed && (ros::Time::now() - last_request > ros::Duration(5.0)))
             {
                 if( arming_client.call(arm_cmd) && arm_cmd.response.success)
                 {
                     ROS_INFO("Vehicle armed");//解锁后打印信息
                 }
                 last_request = ros::Time::now();
             }
             else
             {
                 if(step==0)
                 {
                     local_pos_pub.publish(pose); //先发布第一个点，如果飞机不满足下面这句，就一直会发布这个点
                     if(local_pos.pose.position.z>9.9 && local_pos.pose.position.z<10.1) waiting_time++;
                     if(waiting_time==500) //第一个点的悬停时间为500个单位时，设置下一个点的坐标，
                     {
                         step=1;
                         pose.pose.position.x = 10;
                         pose.pose.position.y = 0;
                         pose.pose.position.z = 10;

                     }
                 }

                 if(step==1)
                 {
                     local_pos_pub.publish(pose); //先发布第二个点，如果飞机不满足下面这句，就一直会发布这个点
                     if(local_pos.pose.position.z>9.9 && local_pos.pose.position.z<10.1) waiting_time++;
                     if(waiting_time==1000)//第二个点的悬停时间也为500个单位时，设置下一个点的坐标，
                     {
                         step=2;
                         pose.pose.position.x = 10;
                         pose.pose.position.y = 10;
                         pose.pose.position.z = 10;

                     }
                 }


                 if(step==2)
                 {
                     local_pos_pub.publish(pose);
                     if(local_pos.pose.position.z>9.9 && local_pos.pose.position.z<10.1) waiting_time++;
                     if(waiting_time==1500)
                     {
                         step=3;
                         pose.pose.position.x = 0;
                         pose.pose.position.y = 10;
                         pose.pose.position.z = 10;

                     }
                 }

                 if(step==3)
                 {
                     local_pos_pub.publish(pose);
                     if(local_pos.pose.position.z>9.9 && local_pos.pose.position.z<10.1) waiting_time++;
                     if(waiting_time==2000)
                     {
                         step=4;
                         pose.pose.position.x = 0;
                         pose.pose.position.y = 0;
                         pose.pose.position.z = 10;

                     }
                 }

                 if(step==4)
                 {
                     local_pos_pub.publish(pose);
                     if(local_pos.pose.position.z>9.9 && local_pos.pose.position.z<10.1) waiting_time++;
                     if(waiting_time==2500)
                     {
                         step=5;

                     }
                 }

                 if(step==5)  //进入自动降落过程
                 {
                     
                     while(ros::ok()) //之所以这里还要设一个while循环，是因为上面的代码没用了，不需要执行上面的代码了，只需要下面的。而如果没有这个
                                      //while，由于外围有一个最大的while，会使得执行完下面的后又去执行99行的内容，也就是又会打开offboard模式
                                      //所以不加这个while就会出现飞机反复在两个模式之间来回切换。加了以后只会执行下面这段。
                     {
		     if( current_state.mode != "AUTO.LAND" && (ros::Time::now() - last_request > ros::Duration(5.0)))
                     {
                         if( set_mode_client.call(offb_set_mode2) && offb_set_mode2.response.mode_sent)
                         {
                             ROS_INFO("AUTO.LAND enabled");//打开模式后打印信息
                         }
                         last_request = ros::Time::now();
                     }
                     local_pos_pub.publish(pose);//这里需要说明，一旦模式为AUTO.LAND，发布位置就不起作用了，这里把这个写在这里是因为px4的机制要求不断地发送位置信息以判断系统
                                                 //为正常的，所以这里依然要写这句话。即使这句话不起作用。
                     ros::spinOnce();  //通常放在一个while循环的最后
                     rate.sleep()//通常放在一个while循环的最后
                     }
                 }

             }

         }
         ros::spinOnce();  //通常放在一个while循环的最后
         rate.sleep();//通常放在一个while循环的最后
     }


     return 0;
 }




//以下是关于这个源码的几点说明
//1.弄清楚需要订阅几个信息，就在开始写几个回调函数
//2.while（ros：：ok（））这个其实就==while（1），一个程序中可以写很多个，不一定只能有一个
//3.ros::spinOnce()与rate.sleep()是成对存在的，写在一起，需要注意：由于我们需要一直运行ros::spinOnce()来实现不停地订阅，所以这两句话要存在于
//  所有的while（ros：：ok（））循环里面，也就是每一个while（ros：：ok（））里面都必须要有这两句话，通常在最后。
//4.注意理解程序中waiting time的含义用法，其实也就是通过累加waiting time使其达到某个值时进入下个点，也就是起到悬停作用
//5.59-64行的代码是为了安全起见，如果没有这句话，飞机一旦连上电，启动树莓派，就直接进入offboard'，危险
//  而加了这句话之后需要飞手将遥控器切到acro模式才能进行后面的代码，更加安全。另外在方针环境里面，
//  这段代码的加入使得运行节点后飞机不会自动起飞，需要在方针软件的终端输入：commander mode acro才能让飞机起飞
//  即在方针环境里commander mode acro的作用是模拟遥控器打开acro模式。
//
//
//
//
//
//
//
//
//






























