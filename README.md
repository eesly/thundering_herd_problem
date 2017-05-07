# thundering_herd_problem

主要写了三个例子，server_accept.cpp和server_epoll.cpp分别验证accept和epoll的惊群问题，server_epoll_thp.cpp是仿照nginx避免惊群的方式的一个简单实现。更多说明见[nginx如何解决惊群效应](http://www.jianshu.com/p/21c3e5b99f4a)

### get start
g++ -o server_accept server_accept.cpp -pthread -g  
g++ -o server_epoll server_epoll.cpp -pthread -g  
g++ -o server_epoll_thp server_epoll_thp.cpp -pthread -g  

usage: ./server_*** [-i ip] [-p port] [-ap] [-n proc_num]  
  -a set reuse addr  
  -p set reuse port  
  -i set bind addr, default value is INADDR_ANY  
  -p set bind port, default value is 9000  
  -n set process num, default value is 1  

### test
- accept  
./server_accept -n 2 -a   
ab -c 1 -n 5 127.0.0.1:9000/

- epoll  
./server_epoll -n 2 -a  
ab -c 1 -n 5 127.0.0.1:9000/

- epoll_thp  
./server_epoll -n 2 -a  
ab -c 1 -n 5 127.0.0.1:9000/
