int hilink_tcp_read(int fd, unsigned char* buf, unsigned short len)
{
    int ret = -1;

    if(buf == NULL)
    {
        return HILINK_SOCKET_NULL_PTR;
    }

	/*TCP\u8bfb\u53d6\u6570\u636e\u9700\u8981\u5224\u65ad\u9519\u8bef\u7801*/
    ret = (int)(recv(fd, buf, len, MSG_DONTWAIT));
    if(ret <= 0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
            return HILINK_SOCKET_NO_ERROR;
        }
        else
        {
            return HILINK_SOCKET_READ_TCP_PACKET_FAILED;
        }
    }

    return ret;
}
