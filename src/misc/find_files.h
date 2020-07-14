#ifndef __MISC_FIND_FILES_H__
#define __MISC_FIND_FILES_H__

struct find_files
{
	char volume[VOLUME_NAME_LENGTH];
	char filesize[NUMBER_STRING_LENGTH];
	char filepath[ABIO_FILE_LENGTH];
	char filename[ABIO_FILE_LENGTH];
};

int RecvReply(va_sock_t sock, char msg[DSIZ * DSIZ], int flag)
{
	int r;
	int b;

	r = 0;

	while ((b = recv(sock, msg + r, 1, 0)) > 0)
	{
		if (msg[r] == '\0')
		{
			r++;
			break;
		}
		else
		{
			r++;
		}
	}

	if (b < 0)
	{
		return b;
	}

	return r;
}

char *stristr(const char *string,const char *strSearch)
{
     const char *s, *sub;
 
     for (;*string;string++) 
	 {
          for (sub=strSearch,s=string;*sub && *s;sub++,s++) 
		  {
              if (tolower(*s) != tolower(*sub)) 
			  {
				  break;
			  }
          }
		  if (*sub == 0)
		  {
			  return (char *)string;
		  }
     }
     return NULL;
}

int InitProcess(char * filename);
int GetArgument(int argc, char ** argv);
int SendCommand();
int SendFindCatalogDBCommand();
int RecvReply(va_sock_t sock, char msg[DSIZ * DSIZ], int flag);
void PrintUsage(char * cmd);
char *stristr(const char *string,const char *strSearch);
#endif
