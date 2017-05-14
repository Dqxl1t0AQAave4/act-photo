#pragma once


#include <Windows.h>
#include <afxwin.h>

#include <vector>
#include <list>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <exception>
#include <cassert>

#include "logger.h"
#include "act-photo.h"



// ======================= byte_buffer ===================================


/*
 * Inspired by the `java.nio.ByteBuffer`.
 */

class byte_buffer
{

private:

    std::vector<char> _data0;
    std::vector<char> _data;

    std::size_t _position;
    std::size_t _limit;

    char * tmp()
    {
        return this->_data0.data();
    }

public:

    byte_buffer(std::size_t initial = 0)
        : _position(0)
        , _limit(initial)
        , _data(initial)
        , _data0(initial)
    {
    }

    char * buffer()
    {
        return this->_data.data();
    }

    char * data()
    {
        return buffer() + position();
    }

    byte_buffer & flip()
    {
        limit(position()).position(0);
        return *this;
    }

    std::size_t position() const
    {
        return this->_position;
    }

    byte_buffer & position(std::size_t new_position)
    {
        this->_position = new_position;
        return *this;
    }

    byte_buffer & increase_position(std::size_t increment)
    {
        position(position() + increment);
        return *this;
    }

    std::size_t capacity() const
    {
        return this->_data.size();
    }

    byte_buffer & capacity(std::size_t new_capacity)
    {
        this->_data.resize(new_capacity);
        this->_data0.resize(new_capacity);
        return *this;
    }

    std::size_t remaining() const
    {
        return limit() - position();
    }

    std::size_t limit() const
    {
        return this->_limit;
    }

    byte_buffer & limit(std::size_t new_limit)
    {
        this->_limit = new_limit;
        return *this;
    }

    byte_buffer & clear()
    {
        position(0);
        return *this;
    }

    byte_buffer & reset()
    {
        position(0).limit(capacity());
        return *this;
    }

    byte_buffer & compact()
    {
        std::size_t remains = remaining();
        if (remains == 0)
        {
            position(0).limit(capacity());
            return *this;
        }
        memcpy_s(tmp(), capacity(), data(), remains);
        memcpy_s(buffer(), capacity(), tmp(), remains);
        position(remains).limit(capacity());
        return *this;
    }

    std::size_t put(const char *in, std::size_t size)
    {
        std::size_t new_position = position() + size;
        std::size_t remains = remaining();
        if (new_position > limit())
        {
            if (remains != 0)
            {
                memcpy_s(data(), remains, in, remains);
                position(limit());
                return size - remains;
            }
            else
            {
                return size;
            }
        }
        else
        {
            memcpy_s(data(), remains, in, size);
            increase_position(size);
            return 0;
        }
    }

    std::size_t get(char *out, std::size_t size)
    {
        std::size_t new_position = this->_position + size;
        std::size_t remains = remaining();
        if (new_position > limit())
        {
            if (remains != 0)
            {
                memcpy_s(out, size, data(), remains);
                position(limit());
                return size - remains;
            }
            else
            {
                return size;
            }
        }
        else
        {
            memcpy_s(out, size, data(), size);
            increase_position(size);
            return 0;
        }
    }

    bool get(char & byte)
    {
        return get(&byte, 1) == 0;
    }
};


// ======================= com_port ======================================


class com_port
{
    
private:

    HANDLE  comm;
    CString comm_name;

public:

    com_port()
        : comm(INVALID_HANDLE_VALUE)
    {
    }
      
    com_port(const com_port &other) = delete;

    com_port(com_port &&other)
        : comm(other.comm)
        , comm_name(other.comm_name)
    {
        other.comm = INVALID_HANDLE_VALUE;
        other.comm_name = _T("");
    }

    com_port & operator = (const com_port &other) = delete;

    com_port & operator = (com_port &&other)
    {
        if (open())
        {
            close();
        }
        this->comm = other.comm;
        this->comm_name = other.comm_name;
        other.comm = INVALID_HANDLE_VALUE;
        other.comm_name = _T("");
        return *this;
    }

    ~com_port()
    {
        close();
    }

    bool open()
    {
        return (comm != INVALID_HANDLE_VALUE);
    }

    bool operator () ()
    {
        return open();
    }

    bool open(CString name)
    {
        if (open() && (comm_name == name))
        {
            logger::log(_T("port [%s] already opened"), name);
            return true;
        }
        if (open())
        {
            if (!close())
            {
                return false;
            }
            return open0(name);
        }
        else
        {
            return open0(name);
        }
    }

    bool close()
    {
        if (!open())
        {
            return true;
        }
        if (!CloseHandle(comm))
        {
            logger::log(_T("cannot close port [%s] handle"), comm_name);
            return false;
        }
        comm = INVALID_HANDLE_VALUE;
        logger::log(_T("port [%s] closed"), comm_name);
        comm_name = "";
        return true;
    }

public:

    bool read(byte_buffer &dst)
    {
        if (!open())
        {
            logger::log(_T("cannot read from closed port"));
            return false;
        }
        DWORD bytes_read;
        if (!ReadFile(comm, dst.data(), dst.remaining(), &bytes_read, NULL))
        {
            logger::log(_T("error while reading the data... closing port [%s]"), comm_name);
            close();
            return false;
        }
        dst.increase_position(bytes_read);
        return true;
    }

    bool write(byte_buffer &src)
    {
        if (!open())
        {
            logger::log(_T("cannot write to closed port"));
            return false;
        }
        DWORD bytes_written;
        if (!WriteFile(comm, src.data(), src.remaining(), &bytes_written, NULL))
        {
            logger::log(_T("error while writing the data... closing port [%s]"), comm_name);
            close();
            return false;
        }
        src.increase_position(bytes_written);
        return true;
    }

private:

    bool open0(CString name)
    {
        comm_name = name;
        comm = CreateFile(
            name,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
        if (comm == INVALID_HANDLE_VALUE)
        {
            if (GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                logger::log(_T("serial port [%s] does not exist"), name);
            }
            else
            {
                logger::log(_T("error occurred while opening [%s] port"), name);
            }
            return false;
        }

        SetCommMask(comm, EV_RXCHAR);
        SetupComm(comm, 1500, 1500);

        COMMTIMEOUTS CommTimeOuts;
        CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
        CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
        CommTimeOuts.ReadTotalTimeoutConstant = 1000;
        CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
        CommTimeOuts.WriteTotalTimeoutConstant = 1000;

        if (!SetCommTimeouts(comm, &CommTimeOuts))
        {
            CloseHandle(comm);
            comm = INVALID_HANDLE_VALUE;
            logger::log(_T("cannot setup port [%s] timeouts"), name);
            return false;
        }

        DCB ComDCM;

        memset(&ComDCM, 0, sizeof(ComDCM));
        ComDCM.DCBlength = sizeof(DCB);
        GetCommState(comm, &ComDCM);
        ComDCM.BaudRate = DWORD(4800);
        ComDCM.ByteSize = 8;
        ComDCM.Parity = ODDPARITY;
        ComDCM.StopBits = ONESTOPBIT;
        ComDCM.fAbortOnError = TRUE;
        ComDCM.fDtrControl = DTR_CONTROL_DISABLE;
        ComDCM.fRtsControl = RTS_CONTROL_DISABLE;
        ComDCM.fBinary = TRUE;
        ComDCM.fParity = TRUE;
        ComDCM.fInX = FALSE;
        ComDCM.fOutX = FALSE;
        ComDCM.XonChar = 0;
        ComDCM.XoffChar = (unsigned char) 0xFF;
        ComDCM.fErrorChar = FALSE;
        ComDCM.fNull = FALSE;
        ComDCM.fOutxCtsFlow = FALSE;
        ComDCM.fOutxDsrFlow = FALSE;
        ComDCM.XonLim = 128;
        ComDCM.XoffLim = 128;

        if (!SetCommState(comm, &ComDCM))
        {
            CloseHandle(comm);
            comm = INVALID_HANDLE_VALUE;
            logger::log(_T("cannot setup port [%s] configuration"), name);
            return false;
        }

        logger::log(_T("successfully connected to [%s] port"), name);

        return true;
    }
};


// ======================= worker ========================================


class worker_stopped
    : public std::runtime_error
{

public:

    worker_stopped()
        : std::runtime_error("")
    {
    }
};


class worker
{

public:

    using mutex_t = std::mutex;
    using guard_t = std::lock_guard < mutex_t > ;

private:

    using ulock_t     = std::unique_lock < mutex_t > ;
    using condition_t = std::condition_variable;

    std::thread                       worker_thread;
                                      
    mutex_t                           mutex;
    condition_t                       cv;

    // guarded by `mutex`
    bool                              working;
    com_port                          port;
    bool                              port_changed;
    std::size_t                       buffer_size;
    std::vector<act_photo::command_t> commands;

    // thread-local
    com_port                          current_port;
    byte_buffer                       buffer;

public:

    mutex_t                           queue_mutex;

    // guarded by `queue_mutex`
    std::size_t                       queue_length;
    std::list<act_photo::packet_t>    packet_queue;

public:

    worker(std::size_t buffer_size = 5000,
           std::size_t queue_length = 1000)
           : buffer(buffer_size)
           , buffer_size(buffer_size)
           , queue_length(queue_length)
    {
        worker_thread = std::thread(&worker::start, this);
    }

    ~worker()
    {
        stop();
    }

    void supply_port(com_port port)
    {
        {
            guard_t guard(mutex);
            this->port = std::move(port);
            this->port_changed = true;
        }
        cv.notify_one();
    }

    void supply_command(act_photo::command_t command)
    {
        {
            guard_t guard(mutex);
            commands.push_back(command);
        }
    }

    void supply_buffer_size(std::size_t buffer_size)
    {
        {
            guard_t guard(mutex);
            this->buffer_size = buffer_size;
        }
    }

    void supply_queue_length(std::size_t queue_length)
    {
        {
            guard_t guard(queue_mutex);
            this->queue_length = queue_length;
        }
    }

    void stop()
    {
        {
            guard_t guard(mutex);
            working = false;
        }
        cv.notify_one();
    }

    void join()
    {
        worker_thread.join();
    }

private:

    void start()
    {
        try
        {
            {
                guard_t guard(mutex);
                this->working = true;
                this->port_changed = false;
            }

            loop();
        }
        catch (const worker_stopped &)
        {
            current_port.close();
            {
                guard_t guard(mutex);
                port.close();
            }
            return;
        }
    }

    com_port & fetch_port()
    {
        ulock_t guard(mutex);
        if (port_changed)
        {
            current_port = std::move(port);
            port_changed = false;
        }
        while (!current_port.open())
        {
            cv.wait(guard, [&] { return port_changed || !working; });
            if (!working)
            {
                throw worker_stopped();
            }
            current_port = std::move(port);
            port_changed = false;
        }
        if (!working)
        {
            throw worker_stopped();
        }
        return current_port;
    }
    
    void loop()
    {
        char packet_bytes[act_photo::packet_body_size];

        std::vector<act_photo::packet_t> packet_buffer;
        packet_buffer.reserve(buffer.capacity() / act_photo::packet_body_size);

        std::list<act_photo::command_t> command_buffer;

        for (;;)
        {
            {
                guard_t guard(mutex);
                buffer.capacity(buffer_size);
            }
            if (!fetch_port().read(buffer))
            {
                continue;
            }

            buffer.flip();

            do
            {
                if (!detect_packet_start()) break;
                buffer.get(packet_bytes, act_photo::packet_body_size);
                packet_buffer.push_back(act_photo::read_packet(packet_bytes));
            }
            while (buffer.remaining() >= act_photo::packet_size);

            buffer.reset();

            {
                guard_t guard(queue_mutex);

                std::size_t can_put = min(queue_length - packet_queue.size(), packet_buffer.size());
                packet_queue.insert(packet_queue.end(),
                                    packet_buffer.begin(), packet_buffer.begin() + can_put);
                packet_buffer.clear();

                command_buffer.insert(command_buffer.end(), commands.begin(), commands.end());
                commands.clear();
            }

            while (!command_buffer.empty())
            {
                act_photo::command_t command = command_buffer.front();
                // for simplicity as the buffer size must commonly be much greater
                assert(command.bytes.size() + 2 <= buffer.remaining());
                act_photo::serialize(command, buffer.data());
                buffer.increase_position(command.bytes.size() + 2);
                
                buffer.flip();

                while (fetch_port().write(buffer) && buffer.remaining())
                    ;

                buffer.reset();

                command_buffer.pop_front();
            }
        }
    }

    bool detect_packet_start()
    {
        char c;
        bool detected = false;
        while (!detected)
        {
            if (!buffer.get(c))
            {
                break;
            }
            if (c == act_photo::packet_delimiter)
            {
                if ((buffer.remaining() >= act_photo::packet_size - 1))
                {
                    char checksum = buffer.data()[act_photo::packet_size - 2];
                    char s = act_photo::read_checksum(buffer.data() - 1); // the buffer position is at least 1
                    if (checksum == s)
                    {
                        detected = true;
                        break;
                    }
                }
            }
        }
        if (buffer.remaining() < act_photo::packet_size - 1)
        {
            return false;
        }
        return detected;
    }
};