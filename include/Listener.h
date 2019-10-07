// Listener.h

#ifndef LISTENER_H
#define LISTENER_H

#include <vector>
#include <unistd.h>
//#include <iostream>

class TListener {
public:
	TListener(){}
	virtual ~TListener(){}
	
	virtual bool Process(const std::vector<unsigned char> &buf, int nwrite) = 0;
};

class TRawWriteListener : public TListener {
public:
	TRawWriteListener(int fd) : _fd(fd) {}
	~TRawWriteListener(){close(_fd);}
	
	virtual bool Process(const std::vector<unsigned char> &buf, int nwrite)
	{
//		std::cerr << "fd = " << _fd << std::endl;
		int size = write(_fd, &buf.front(), nwrite);
		return size == nwrite;
	}
	
private:
	int _fd;
};

class TScCalListener : public TListener {
public:
	TScCalListener(int fd) : _fd(fd), _cycle(0){}
	~TScCalListener(){close(_fd);}
	
	virtual bool Process(const std::vector<unsigned char> &buf, int nwrite);
private:
	int _fd;
	int _cycle;
};

#endif
