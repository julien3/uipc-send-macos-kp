#include <iostream>

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define PIPE_BUFFER_SIZE 32768

static volatile sig_atomic_t quit = 0;

void StopConnecting(int) {
  quit = 1;
}

bool Read(int sock, char** msg, unsigned long* msg_size) {
  int message_length = 0;
  int br = 0;
  bool socket_closed = false;
  
  *msg_size = sizeof(int);

  // Read sizeof(int) bytes to get the message length header
  do {
    int num_bytes = recv(sock, &message_length + br, sizeof(int) - br, 0);
    
    if(num_bytes <= 0) {
      socket_closed = true;
    } else {
      br += num_bytes;
    }
  } while(!socket_closed && br < sizeof(int));
  
  if(socket_closed) {
    return false;
  }
  
  if(br == sizeof(int)) {
    br = 0;
  
    if(message_length > 0) {
      *msg = new char[message_length];
      *msg_size = message_length;
      
      // Read bytes until message is complete
      do {
        int num_bytes = recv(sock, *msg + br, *msg_size - br, 0);
        
        if(num_bytes <= 0) {
          socket_closed = true;
        } else {
          br += num_bytes;
        }
      } while (!socket_closed && br < message_length);
        
      if (socket_closed) {
        // Missing data so read is erroneous
        delete[] *msg;
        *msg = NULL;
        return false;
      }
    } else {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

bool Write(int sock, const char* msg, unsigned long msg_size) {
  unsigned long to_send_size = msg_size + sizeof(int);
  char* to_send = new char[to_send_size];
  *reinterpret_cast<int*> (to_send) = static_cast<int> (msg_size);
  memcpy(to_send+sizeof(int), msg, msg_size);
  
  // Compute how many chunks are necessary
  int full_chunk_count = to_send_size / PIPE_BUFFER_SIZE;
  int residual_chunk_size = to_send_size % PIPE_BUFFER_SIZE;
  unsigned long bytes_written = 0;
  unsigned long total_bytes_written = 0;
  
  bool write_failed = false;
  
  // Send full chunks
  for (int i=0; i<full_chunk_count; i++) {
    bytes_written = send(sock, to_send + i * PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, MSG_NOSIGNAL);
    if (bytes_written == PIPE_BUFFER_SIZE) {
      total_bytes_written += PIPE_BUFFER_SIZE;
    } else {
      write_failed = true;
      break;
    }
  }

  if(write_failed) {
    delete[] to_send;
    return false;
  }

  // Send residual chunk
  if (residual_chunk_size != 0) {
    bytes_written = send(sock, to_send + full_chunk_count * PIPE_BUFFER_SIZE, residual_chunk_size, MSG_NOSIGNAL);
    if (bytes_written == residual_chunk_size) {
      total_bytes_written += residual_chunk_size;
    } else {
      write_failed = true;
    }
  }

  delete[] to_send;
  
  if(write_failed) {
    return false;
  }
  
  return total_bytes_written == to_send_size;
}

int main(int argc, char* argv[]) {
  // Catch ctrl+c
  struct sigaction sa = { StopConnecting };
  sigaction(SIGINT, &sa, NULL);
  
  std::string local_pipe_name = "./KPrepropipe";
  int con_count = 0;
  
  while(!quit) {
    int sock;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      return 1;
    }
    
    int conn_res = 0;
    struct sockaddr_un remote = {0};
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, local_pipe_name.c_str());
    
    int val = 1;
    int r = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void*)&val, sizeof(val));
    if(r != 0) {
      std::cerr << "Failed while setting no sigpipe option to domain socket client " << local_pipe_name << std::endl;
      return 1;
    }
      
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
      std::cerr << "Can't set recv timeout";
      return 1;
    }

    if(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
      std::cerr << "Can't set send timeout";
      return 1;
    }
    
    int res = connect(sock, (struct sockaddr*) &remote, sizeof(remote));
    
    if(res == -1) {
      std::cerr << "Failed while connecting to domain socket " << local_pipe_name << std::endl;
      close(sock);
      return 1;
    }
    
    int con_index = con_count++;
    std::cout << "Connected (#" << con_index << ")" << std::endl;
    
    char buf[] = "hello";
    if(!Write(sock, buf, sizeof(buf))) {
      std::cerr << "Failed while writing on connection #" << con_index << std::endl;
      close(sock);
      return 1;
    }
    
    // Uncomment to check server's response is received correctly
    /*char* msg = NULL;
    unsigned long msg_length = 0;
    
    if(!Read(sock, &msg, &msg_length)) {
      std::cerr << "Failed while reading on connection #" << con_index << std::endl;
      close(sock);
      return 1;
    }
    
    if(msg) {
      std::cout << "Read #" << con_index << ": " << msg << std::endl;
      delete [] msg;
    }*/
    
    close(sock);
    
    std::cout << "Disconnected (#" << con_index << ")" << std::endl;
    
    usleep(1000);
  }
}
