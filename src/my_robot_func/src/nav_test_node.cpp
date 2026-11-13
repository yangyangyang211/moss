#include<ros/ros.h>
#include<move_base_msgs/MoveBaseAction.h>
#include<actionlib/client/simple_action_client.h>
#include <wpb_home_behaviors/Coord.h>
#include <sound_play/SoundRequest.h>
#include <std_msgs/String.h>
typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
static double face_x = 0.0;
static double face_y = 0.0;
static bool has_face = false;
static bool waiting_for_confirmation = false;
static ros::Publisher spk_pub;

void KeywordCB(const std_msgs::String::ConstPtr& msg)
{
    if (!waiting_for_confirmation)
    {
        return;
    }

    // 先检查否定回答，避免把“不是”中的“是”当成同意。
    const std::string& answer = msg->data;
    if (answer.find("不") != std::string::npos ||
        answer.find("否") != std::string::npos ||
        answer.find("取消") != std::string::npos)
    {
        waiting_for_confirmation = false;
        ROS_INFO("已取消录入人脸信息");
    }
    else if (answer.find("是") != std::string::npos)
    {
        waiting_for_confirmation = false;
        std_msgs::String strSpeak;
        strSpeak.data = "正在录入人脸信息，请稍等";
        spk_pub.publish(strSpeak);
        // TODO: 在这里调用实际的人脸录入功能。
    }
}

void FaceCoordCB(const wpb_home_behaviors::Coord::ConstPtr& msg)
{
    if (msg->name.empty())
    {
        has_face = false;
        return;  // 当前消息没有有效人脸
    }

    // 示例：读取当前消息中的第一张人脸
    const double x = msg->x[0];
    const double y = msg->y[0];
    const double z = msg->z[0];

    ROS_INFO("Face %s, map position: (%.2f, %.2f, %.2f)",
             msg->name[0].c_str(), x, y, z);
    face_x = x;
    face_y = y;
    has_face = true;
    // 在这里使用坐标
}
int main(int argc, char **argv)
{
    ros::init(argc, argv, "nav_test_node");
    ros::NodeHandle nh;

    MoveBaseClient ac("move_base", true);
    ros::Subscriber face_sub = nh.subscribe(
        "/wpb_home/face_detect_3d_map", 10, FaceCoordCB);
    ros::Publisher tts_pub = nh.advertise<sound_play::SoundRequest>("/robotsound", 20);
    ros::Subscriber sub_sr = nh.subscribe("/xfyun/iat", 10, KeywordCB);
    spk_pub = nh.advertise<std_msgs::String>("/xfyun/tts", 10);

    ros::Rate loop_rate(10);

    // 上次发送时间：只在循环外初始化一次
    ros::Time last_send_time = ros::Time::now();
    bool navigating = false;

    while (ros::ok())
    {
        ros::spinOnce();

        // 检查结果，不阻塞等待
        if (navigating && ac.getState().isDone())
        {
            if (ac.getState() ==
                actionlib::SimpleClientGoalState::SUCCEEDED)
            {
                ROS_INFO("okkkkkkkk");
                sound_play::SoundRequest sp;
                sp.sound = sound_play::SoundRequest::SAY;
                sp.command = sound_play::SoundRequest::PLAY_ONCE;
                sp.volume = 1.5;      //在kinetic及以上版本ROS(Ubuntu16.04+)中,需要这句赋值音量才能发声
                sp.arg = "导航成功，是否录入人脸信息，请回答是或否";
                waiting_for_confirmation = true;
                tts_pub.publish(sp);
            }
            else
            {
                ROS_INFO("nooooooooo");
                sound_play::SoundRequest sp;
                sp.sound = sound_play::SoundRequest::SAY;
                sp.command = sound_play::SoundRequest::PLAY_ONCE;
                sp.volume = 1.5;      //在kinetic及以上版本ROS(Ubuntu16.04+)中,需要这句赋值音量才能发声
                sp.arg = "主人，请稍等";
                tts_pub.publish(sp);
            }
            navigating = false;
        }

        // 当前时间：每轮更新
        ros::Time now = ros::Time::now();

        if (has_face && !navigating && !waiting_for_confirmation &&
            ac.isServerConnected() &&
            (now - last_send_time).toSec() >= 10.0)
        {
            move_base_msgs::MoveBaseGoal goal;
            goal.target_pose.header.frame_id = "map";
            goal.target_pose.header.stamp = now;
            goal.target_pose.pose.position.x = face_x;
            goal.target_pose.pose.position.y = face_y;
            goal.target_pose.pose.orientation.w = 1.0;

            ac.sendGoal(goal);

            // 发送后重新开始计时
            last_send_time = now;
            navigating = true;
        }

        loop_rate.sleep();
    }

    return 0;
}
