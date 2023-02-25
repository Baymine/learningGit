#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <functional>

namespace snippet
{
  using namespace std;
  // This function splits a given string into multiple tokens and adds them to a vector
  void split(const string &s, vector<string> &tokens, const string &delimiters = " ")
  {
    // Initialize the starting positions of the last and current position
    string::size_type lastPos = s.find_first_not_of(delimiters, 0);
    string::size_type pos = s.find_first_of(delimiters, lastPos);

    // While there are still tokens to be found in the string
    while (string::npos != pos || string::npos != lastPos)
    {
      // Add the substring from the last position to the current one to the tokens vector
      tokens.push_back(s.substr(lastPos, pos - lastPos));
      // Update the last and current positions
      lastPos = s.find_first_not_of(delimiters, pos);
      pos = s.find_first_of(delimiters, lastPos);
    }
  }
  void printMessage(const std::string &message, std::function<void()> callback)
  {
    std::cout << "Thread " << std::this_thread::get_id() << " is processing..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "Thread " << std::this_thread::get_id() << ": " << message << std::endl;
    callback();
  }

  int main()
  {
    std::cout << "Main thread " << std::this_thread::get_id() << " started" << std::endl;
    auto callback = []()
    { std::cout << "Callback called from thread " << std::this_thread::get_id() << std::endl; };
    printMessage("Hello, World!", callback);
    std::cout << "Main thread " << std::this_thread::get_id() << " continues to run" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Main thread " << std::this_thread::get_id() << " is finished" << std::endl;
    return 0;
  }
}

namespace test
{
  // void long_running_task(int duration, std::function<void(int)> callback) {
  //     // callback(duration);
  //     std::this_thread::sleep_for(std::chrono::seconds(duration));
  //     callback(duration);
  // }

  // void task_callback(int duration) {
  //     std::cout << "Task finished after " << duration << " seconds." << std::endl;
  // }

  // int main() {
  //     std::cout << "Starting task." << std::endl;
  //     long_running_task(3, task_callback);
  //     std::cout << "Task started, returning control to main function." << std::endl;
  //     std::this_thread::sleep_for(std::chrono::seconds(1));
  //     std::cout << "Main function finished." << std::endl;
  //     return 0;
  // }

#include <iostream>
#include <functional>
#include <chrono>
#include <thread>

  void doTask1(std::function<void(int)> callback)
  {
    std::cout << "Starting task: 1" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Task 1 completed" << std::endl;
    callback(42);
  }

  void doTask2(std::function<void(int)> callback)
  {
    std::cout << "Starting task: 2" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Task 2 completed" << std::endl;
    callback(43);
  }

  int main()
  {
    std::cout << "Starting tasks" << std::endl;
    doTask1([](int result)
            {
    std::cout << "Task 1 callback completed with result: " << result << std::endl;
    doTask2([](int result) {
      std::cout << "Task 2 callback completed with result: " << result << std::endl;
    }); });
    std::cout << "Tasks started" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
  }

} // namespace test

int main()
{
  test::main();
}
