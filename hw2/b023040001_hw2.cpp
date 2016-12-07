#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <map>
#include <queue>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <unistd.h>
using namespace std;

// define maximum of clients as n*3, n clients for each group
#define MAX_CLIENT_NUM 8*3
#define MAX_BUFFER_LEN 128+3
#define MAX_USERNAME_LEN 20
#define MAX_FILENAME_LEN 30
#define MAX_COMMAND_LEN 20
#define MAX_PERMISSION_LEN 6+1

struct status_node
{
	char status;
	int count;
};

struct object
{
	char filename[MAX_FILENAME_LEN];
	char permission;
};

class capability_list_node
{
public:
	capability_list_node()
	{
		memset( username, 0, MAX_USERNAME_LEN );
	}
	char username[MAX_USERNAME_LEN];
	int group;
	deque<struct object> list;
	void push( const char* filename, const char permission )
	{
		struct object temp;
		memset( temp.filename, 0, MAX_FILENAME_LEN );
		strncpy( temp.filename, filename, MAX_FILENAME_LEN - 1 );
		temp.permission = permission;
		list.push_back( temp );
	}
	int search( const char* target_file )
	{
		for( int i = 0 ; i < list.size() ; i++ )
		{
			if( strncmp( list[i].filename, target_file, strlen( list[i].filename ) ) == 0 )
				return i;
		}
		return -1;
	}
	int myself( const char* name )
	{
		if( strncmp( name, username, strlen( name ) ) == 0 )
			return 1;
		return 0;
	}
	void set( const char* name, const int g )
	{
		memset( username, 0, MAX_USERNAME_LEN );
		strncpy( username, name, MAX_USERNAME_LEN );
		group = g;
	}
};

class capability_list_node capability_list[MAX_CLIENT_NUM];
map< string, struct status_node > files_status;
int usernum;

pthread_mutex_t lock_user_list;
pthread_mutex_t lock_operated_file;
pthread_mutex_t lock_fopen;

void run_srv( uint16_t srv_port );
void run_cli( string srv_ip, uint16_t srv_port );
void write_all( int fd, char buffer_write[] );
int read_all( int fd, char buffer_read[] );
int identify_command( char *command );

void *srv_to_cli( void *args )
{
	int fd = *( int* )args;	
	
	char buffer_write[MAX_BUFFER_LEN], buffer_read[MAX_BUFFER_LEN], username[MAX_USERNAME_LEN];

	//user login
	FILE *user_list;

	read_all( fd, buffer_read );
	strncpy( username, buffer_read + 3, MAX_USERNAME_LEN - 1 );

	FILE *file, *file1;
	char buffer[MAX_BUFFER_LEN];
	int find = 0, group, temp_i;
	pthread_mutex_lock( &lock_user_list );
	if( ( user_list = fopen( "srv/user_list", "a+" ) ) == NULL )
	{
		fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
	    perror("");
		pthread_mutex_unlock( &lock_user_list );
    	exit( 1 );
	}
	else
	{
		rewind( user_list );
		while( ( fscanf( user_list, "%d %s", &group, buffer_read ) ) == 2 )
		{
			if( strncmp( buffer_read, username, strlen( username ) ) == 0 )
			{
				cout << " [S] Old user : " << username << endl;
				find = 1;

				memset( buffer_write, 0, MAX_BUFFER_LEN );
				sprintf( buffer_write + 3, "%d", group );
				write_all( fd, buffer_write );
				break;
			}
		}
		if( !find )
		{
			memset( buffer_write, 0, MAX_BUFFER_LEN );
			sprintf( buffer_write + 3, "new " );
			write_all( fd, buffer_write );

			read_all( fd, buffer_read );
			sscanf( buffer_read + 3, "%d", &group );

			fseek( user_list, 0, SEEK_END );
			fprintf( user_list, "%d %s\n", group, username );
			fclose( user_list );
			cout << " [S] New user : " << username << endl;
			
			memset( buffer_read, 0, MAX_BUFFER_LEN );
			sprintf( buffer_read, "rm -rf cli/%s; cd cli; mkdir %s", username, username );
			system( buffer_read );

			capability_list[usernum].set( username, group );

			
			if( ( ( file = popen( "cd srv;ls .[^.]*", "r" ) ) == 0 ) || ( ( file1 = popen( "cat `ls srv/.[^.]*`", "r" ) ) == 0 ) )
		    {
		        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
		        perror("");
		        exit( 1 );
		    }

		    memset( buffer, 0, MAX_BUFFER_LEN );
		    char temp_c;
            while( fscanf( file, "%s", buffer ) == 1 )
            {
                    memset( buffer_write, 0, MAX_BUFFER_LEN );memset( buffer_read, 0, MAX_BUFFER_LEN );
                    fscanf( file1, "%s %s %d", buffer_write, buffer_read, &temp_i );
                    cout << buffer_write << " " << buffer_read << " " << temp_i << endl;
                    temp_c = 0;
                    if( capability_list[usernum].myself( buffer_read ) )
                    {
                        if( buffer_write[0] == 'r' && buffer_write[1] == 'w' )
                            capability_list[usernum].push( buffer + 1, 'a' );
                        else if( buffer_write[0] == 'r' )
                            capability_list[usernum].push( buffer + 1, 'r');
                        else if( buffer_write[1] == 'w' )
                            capability_list[usernum].push( buffer + 1, 'w');
                        else
                            capability_list[usernum].push( buffer + 1, 'c');
                    }
                    if( capability_list[usernum].group == temp_i && !capability_list[usernum].myself( buffer_read ) )
                    {
                        if( buffer_write[2] == 'r' && buffer_write[3] == 'w' )
                            capability_list[usernum].push( buffer + 1, 'a');
                        else if( buffer_write[2] == 'r' )
                            capability_list[usernum].push( buffer + 1, 'r');
                        else if( buffer_write[3] == 'w' )
                            capability_list[usernum].push( buffer + 1, 'w');
                    }
                    if( capability_list[usernum].group != temp_i )
		            {
          			    if( buffer_write[4] == 'r' && buffer_write[5] == 'w' )
		                    capability_list[usernum].push( buffer + 1, 'a');
           			    else if( buffer_write[4] == 'r' )
		                    capability_list[usernum].push( buffer + 1, 'r');
           			    else if( buffer_write[5] == 'w' )
		                    capability_list[usernum].push( buffer + 1, 'w');
           			}
		    }
			fclose( file );
			fclose( file1 );
			usernum++;
		}
	}
	pthread_mutex_unlock( &lock_user_list );

	//identify the command
	char *temp, command[MAX_COMMAND_LEN], filename[MAX_FILENAME_LEN], permission[MAX_PERMISSION_LEN], owner[MAX_USERNAME_LEN], write_mode;
	string current_file;
	int access, file_group;
	FILE *current_operated_file;
	cout << " * Read command : ";
	while( read_all( fd, buffer_read ) != 0 )
	{
		puts( buffer_read + 3 );
		temp = strtok( buffer_read + 3, " \t\n\0" );
		if( temp != NULL )
		{
			memset( command, 0, MAX_COMMAND_LEN );
			strncpy( command, temp, MAX_COMMAND_LEN );
			switch( identify_command( command ) )
			{
			//new
			case 0:
				//filename
				temp = strtok( NULL, " \t\n\0" );
				if( temp != NULL )
				{
					memset( filename, 0, MAX_FILENAME_LEN );
					strncpy( filename, temp, MAX_FILENAME_LEN );
					//permission
					temp = strtok( NULL, " \t\n\0" );
					if( temp != NULL )
					{
						memset( permission, 0, MAX_PERMISSION_LEN );
						strncpy( permission, temp, MAX_PERMISSION_LEN );
						permission[6] = '\0';

						memset( buffer_read, 0, MAX_BUFFER_LEN );
						sprintf( buffer_read, "cd srv; touch %s ;echo %s %s %d > .%s", filename, permission, username, group, filename );
						system( buffer_read );

						for( int i = 0, pos = -1 ;  i < MAX_CLIENT_NUM ; i++ )
						{
							if( ( pos = capability_list[i].group != -1 ) )
							{
								if( capability_list[i].search( filename ) == -1 )
								{
									if( capability_list[i].myself( username ) )
						            {
						                if( permission[0] == 'r' && permission[1] == 'w' )
                                            capability_list[i].push( filename, 'a' );
                                        else if( permission[0] == 'r' )
                                            capability_list[i].push( filename, 'r');
                                        else if( permission[1] == 'w' )
                                            capability_list[i].push( filename, 'w');
                                        else
                                            capability_list[i].push( filename, 'c');
                                    }
                                    if( capability_list[i].group == group && !capability_list[i].myself( username ) )
                                    {
                                        if( permission[2] == 'r' && permission[3] == 'w' )
                                            capability_list[i].push( filename, 'a');
                                        else if( permission[2] == 'r' )
                                            capability_list[i].push( filename, 'r');
                                        else if( permission[3] == 'w' )
                                            capability_list[i].push( filename, 'w');
                                    }
                                    if( capability_list[i].group != group )
                                    {
                                        if( permission[4] == 'r' && permission[5] == 'w' )
                                            capability_list[i].push( filename, 'a');
                                        else if( permission[4] == 'r' )
                                            capability_list[i].push( filename, 'r');
                                        else if( permission[5] == 'w' )
                                            capability_list[i].push( filename, 'w');
                                    }
								}
							}
						}

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "ls -l srv/%s ", filename );
						write_all( fd, buffer_write );
					}
					else
					{
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write +3, "echo [C] Wrong command " );
						write_all( fd, buffer_write );
					}
				}
				else
                {
                    memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write +3, "echo [C] Wrong command " );
                    write_all( fd, buffer_write );
                }
				break;
			//read
			case 1:
				//filename
				temp = strtok( NULL, " \t\n\0" );
				if( temp != NULL )
				{
					memset( filename, 0, MAX_FILENAME_LEN );
					strncpy( filename, temp, MAX_FILENAME_LEN );
					current_file = filename;

					memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write, "srv/.%s", filename );
					pthread_mutex_lock( &lock_fopen );
                    if( ( current_operated_file = fopen( buffer_write, "r" ) ) == NULL )
                    {
                        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
                        perror("");
                        exit( 1 );
                    }

                    memset( permission, 0, MAX_PERMISSION_LEN );
                    memset( owner, 0, MAX_USERNAME_LEN );
                    fscanf( current_operated_file, "%s %s %d", permission, owner, &file_group );
                    fclose( current_operated_file );
                    pthread_mutex_unlock( &lock_fopen );

					for( int i = 0, pos = -1 ; i < MAX_CLIENT_NUM ; i++ )
					{
						if( capability_list[i].group != -1 && capability_list[i].myself( username ) )
						{
							if( ( pos = capability_list[i].search( filename ) ) != -1 )
							{
								if( capability_list[i].list[pos].permission == 'a' || capability_list[i].list[pos].permission == 'r' )
								{
									access = 0;

			                        while( !access )
                        			{
            			                pthread_mutex_lock( &lock_operated_file );
			                            switch( files_status[current_file].status )
                        			    {
										case '-':
            			                case 0:
			                                files_status[current_file].status = 'r';
                        			    case 'r':
            			                    files_status[current_file].count++;
			                                access = 1;
                        			        break;
            			                case 'c':
			                            case 'w':
                        			        pthread_mutex_unlock( &lock_operated_file );
            			                    sleep( 1 );
			                                continue;
                        			    }
            			                pthread_mutex_unlock( &lock_operated_file );
			                        }

                        			memset( buffer_write, 0, MAX_BUFFER_LEN );
            			            sprintf( buffer_write, "cp srv/%s cli/%s/%s", filename, username, filename );
			                        system( buffer_write );

                        			memset( buffer_write, 0, MAX_BUFFER_LEN );
            			            sprintf( buffer_write + 3, "ls -l cli/%s/%s ", username, filename );
			                        puts( buffer_write + 3 );
            			            write_all( fd, buffer_write );

			                        pthread_mutex_lock( &lock_operated_file );
			                        if( --files_status[current_file].count == 0 )
            			                files_status[current_file].status = '-';
			                        pthread_mutex_unlock( &lock_operated_file );
								}
								else
								{
                        			memset( buffer_write, 0, MAX_BUFFER_LEN );
            			            sprintf( buffer_write + 3, "echo \" [C] You did not get the access permission ! \" " );
			                        write_all( fd, buffer_write );
								}
							}
							else
                            {
                                memset( buffer_write, 0, MAX_BUFFER_LEN );
                                sprintf( buffer_write + 3, "echo \" [C] You did not get the access permission ! \" " );
                                write_all( fd, buffer_write );
                            }
							break;
						}
					}
/*
					if( ( ( strncmp( owner, username, strlen( username ) ) == 0 ) && ( permission[0] == 'r' ) ) ||
                        ( ( file_group == group ) && ( permission[2] == 'r' ) ) ||
                        ( permission[4] == 'r' ) )
                    {

						access = 0;

						while( !access )
						{
							pthread_mutex_lock( &lock_operated_file );
							switch( files_status[current_file].status )
							{
							case 0:
								files_status[current_file].status = 'r';
							case 'r':
								files_status[current_file].count++;
								access = 1;
								break;
							case 'c':
							case 'w':
								pthread_mutex_unlock( &lock_operated_file );
								sleep( 1 );
								continue;
							}
							pthread_mutex_unlock( &lock_operated_file );
						}

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write, "cp srv/%s cli/%s/%s", filename, username, filename );
						system( buffer_write );

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "ls -l cli/%s/%s ", username, filename );
						puts( buffer_write + 3 );
						write_all( fd, buffer_write );

						pthread_mutex_lock( &lock_operated_file );
						if( --files_status[current_file].count == 0 )
							files_status[current_file].status = '-';
						pthread_mutex_unlock( &lock_operated_file );
					}
					else
					{
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "echo \" [C] You did not get the access permission ! \"" );
						write_all( fd, buffer_write );
					}*/
				}
				else
				{
                    memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write +3, "echo [C] Wrong command " );
                    write_all( fd, buffer_write );
				}
				break;
				//write
				case 2:
				temp = strtok( NULL, " \t\n\0" );
				if( temp != NULL )
				{
					memset( filename, 0, MAX_FILENAME_LEN );
					strncpy( filename, temp, MAX_FILENAME_LEN );

					temp = strtok( NULL, " \t\n\0" );
					if( temp != NULL )
					{
						write_mode = temp[0];

						for( int i = 0, pos = -1 ; i < MAX_CLIENT_NUM ; i++ )
						{
							if( capability_list[i].group != -1 && capability_list[i].myself( username ) && ( pos = capability_list[i].search( filename ) ) != -1 )
							{
								if( capability_list[i].list[pos].permission == 'w' || capability_list[i].list[pos].permission == 'a' )
								{
									access = 0;
		                            current_file = filename;
    		                        while( !access )
        		                    {
            		                    pthread_mutex_lock( &lock_operated_file );
                		                if( files_status[current_file].status == '-' || files_status[current_file].status == 0 )
                    		            {
                        		            files_status[current_file].status = 'w';
                            		        pthread_mutex_unlock( &lock_operated_file );

	                                	    memset( buffer_write, 0, MAX_BUFFER_LEN );
    	                                	if( write_mode == 'o' )
        	                                	sprintf( buffer_write, "cp cli/%s/%s srv", username, filename );
	            	                        else if( write_mode == 'a' )
    	            	                    {
        	            	                    sprintf( buffer_write, "echo `cat cli/%s/%s` >> srv/%s", username, filename, filename );
            	            	            }
                	            	        system( buffer_write );

	                                	    memset( buffer_write, 0, MAX_BUFFER_LEN );
    	                                	sprintf( buffer_write + 3, "ls -l srv/%s ", filename );
	    	                                write_all( fd, buffer_write );

    	    	                            access = 1;
        	    	                        pthread_mutex_lock( &lock_operated_file );
            	    	                    files_status[current_file].status = '-';
                	    	                pthread_mutex_unlock( &lock_operated_file );
                    	    	        }
                        	    	    else
                            	    	{
                                	    	pthread_mutex_unlock( &lock_operated_file );
	                                    	sleep( 1 );
		                                    continue;
    		                            }
        		                    }

								}
								else
		                        {
		                            memset( buffer_write, 0, MAX_BUFFER_LEN );
        		                    sprintf( buffer_write + 3, "echo \" [C] You did not get the permission ! \"" );
                		            write_all( fd, buffer_write );
                        		}
								break;
							}
						}
/*						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write, "srv/.%s", filename );
	                    pthread_mutex_lock( &lock_fopen );
						if( ( current_operated_file = fopen( buffer_write, "r" ) ) == 0 )
						{
    	                    fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
	                        perror("");
                        	exit( 1 );
                   		}

						memset( owner, 0, MAX_USERNAME_LEN );
						memset( permission, 0, MAX_PERMISSION_LEN );
						fscanf( current_operated_file, "%s %s %d", permission, owner, &file_group );
						fclose( current_operated_file );
	                    pthread_mutex_unlock( &lock_fopen );

						

						if( ( ( strncmp( username, owner, strlen( owner ) ) == 0 ) && ( permission[1] == 'w' ) ) ||
							( ( group == file_group ) && ( permission[3] == 'w' ) ) ||
							( permission[5] == 'w' ) )
						{
							access = 0;
							current_file = filename;
							while( !access )
							{
								pthread_mutex_lock( &lock_operated_file );
								if( files_status[current_file].status == '-' || files_status[current_file].status == 0 )
								{
									files_status[current_file].status = 'w';
									pthread_mutex_unlock( &lock_operated_file );

			                        memset( buffer_write, 0, MAX_BUFFER_LEN );
									if( write_mode == 'o' )
				                        sprintf( buffer_write, "cp cli/%s/%s srv", username, filename );
									else if( write_mode == 'a' )
									{
										sprintf( buffer_write, "echo `cat cli/%s/%s` >> srv/%s", username, filename, filename );
									}
									system( buffer_write );

									memset( buffer_write, 0, MAX_BUFFER_LEN );
									sprintf( buffer_write + 3, "ls -l srv/%s ", filename );
									write_all( fd, buffer_write );

									access = 1;
									pthread_mutex_lock( &lock_operated_file );
									files_status[current_file].status = '-';
									pthread_mutex_unlock( &lock_operated_file );
								}
								else
								{
									pthread_mutex_unlock( &lock_operated_file );
									sleep( 1 );
									continue;
								}
							}
						}
						else
						{
							memset( buffer_write, 0, MAX_BUFFER_LEN );
							sprintf( buffer_write + 3, "echo \" [C] You did not get the permission ! \"" );
							write_all( fd, buffer_write ); 
						}*/
					}
					else
					{
        	            memset( buffer_write, 0, MAX_BUFFER_LEN );
    	                sprintf( buffer_write +3, "echo [C] Wrong command " );
	                    write_all( fd, buffer_write );
					}
				}
				else
                {
                    memset( buffer_write, 0, MAX_BUFFER_LEN );
                    sprintf( buffer_write +3, "echo [C] Wrong command " );
                    write_all( fd, buffer_write );
                }
				break;
				//change
				case 3:
					temp = strtok( NULL, " \t\n\0" );
					if( temp != NULL )
					{
						memset( filename, 0, MAX_FILENAME_LEN );
						strncpy( filename, temp, MAX_FILENAME_LEN );

						temp = strtok( NULL, " \t\n\0" );
						if( temp != NULL )
						{
							memset( permission, 0, MAX_PERMISSION_LEN );
							strncpy( permission, temp, MAX_PERMISSION_LEN );

							memset( buffer_write, 0, MAX_BUFFER_LEN );
							sprintf( buffer_write, "srv/.%s", filename );
		                    pthread_mutex_lock( &lock_fopen );
							if( ( current_operated_file = fopen( buffer_write, "r" ) ) == 0 )
							{
								fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
						        perror("");
						        exit( 1 );
							}

							memset( owner, 0, MAX_USERNAME_LEN );
							fscanf( current_operated_file, "%*s %s %d", owner, &file_group );
							fclose( current_operated_file );
		                    pthread_mutex_unlock( &lock_fopen );

							for( int i = 0, pos = -1 ; i < MAX_CLIENT_NUM ; i++ )
							{
								if( capability_list[i].group != -1 && capability_list[i].myself( username ) && ( pos = capability_list[i].search( filename ) ) != -1 )
								{
									access = 0;
                                	current_file = filename;
                            	    while( !access )
                        	        {
                    	                pthread_mutex_lock( &lock_operated_file );
                	                    if( files_status[current_file].status == '-' || files_status[current_file].status == 0 )
            	                        {
        	                                files_status[current_file].status = 'c';
    	                                    pthread_mutex_unlock( &lock_operated_file );

	                                        memset( buffer_write, 0, MAX_BUFFER_LEN );
                                        	sprintf( buffer_write, "srv/.%s", filename );
                                    	    pthread_mutex_lock( &lock_fopen );
                                	        if( ( current_operated_file = fopen( buffer_write, "w" ) ) == 0 )
                            	            {
                        	                    fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
                    	                        perror("");
                	                            exit( 1 );
            	                            }
        	                                fprintf( current_operated_file, "%s %s %d\n", permission, username, group );
    	                                    fclose( current_operated_file );
	                                        pthread_mutex_unlock( &lock_fopen );

                                        	access = 1;
                                    	    pthread_mutex_lock( &lock_operated_file );
                                	        files_status[current_file].status = '-';
                            	            pthread_mutex_unlock( &lock_operated_file );
                        	            }
                    	                else
            	                        {
                	                        pthread_mutex_unlock( &lock_operated_file );
        	                                sleep( 1 );
    	                                }
	                                }

									memset( buffer_write, 0, MAX_BUFFER_LEN );
    	                            sprintf( buffer_write + 3, "cat srv/.%s ", filename );
	                                write_all( fd, buffer_write );

									for( int i1 = 0, pos1 = -1 ; i1 < MAX_CLIENT_NUM ; i1++ )
									{puts(capability_list[i1].username);cout<<capability_list[i1].group<<endl;
										if( capability_list[i1].group != -1 )
										{
											if( pos1 = capability_list[i1].search( filename ) != -1 )
											{
												if( capability_list[i1].myself( username ) )
			                                    {
    	    		                                if( permission[0] == 'r' && permission[1] == 'w' )
        	        		                            capability_list[i1].list[pos1].permission = 'a';
            	            		                else if( permission[0] == 'r' )
                	                		            capability_list[i1].list[pos1].permission = 'r';
                    	                    		else if( permission[1] == 'w' )
		                	                            capability_list[i1].list[pos1].permission = 'w';
        		            	                    else
                		        	                    capability_list[i1].list[pos1].permission = 'c';
                        		    	        }
                                			    if( capability_list[i1].group == group && !capability_list[i1].myself( username ) )
		                                    	{	
        		                                	if( permission[2] == 'r' && permission[3] == 'w' )
                		                            	capability_list[i1].list[pos1].permission = 'a';
	                        		                else if( permission[2] == 'r' )
    	                            		            capability_list[i1].list[pos1].permission = 'r';
        	                                		else if( permission[3] == 'w' )
		    	                                        capability_list[i1].list[pos1].permission = 'w';
													else
														capability_list[i1].list.erase( capability_list[i1].list.begin() + pos1 );
        		        	                    }
                		    	                if( capability_list[i1].group != group )
                        			            {
                                			        if( permission[4] == 'r' && permission[5] == 'w' )
                                        			    capability_list[i1].list[pos1].permission = 'a';
		                                    	    else if( permission[4] == 'r' )
        		                                	    capability_list[i1].list[pos1].permission = 'r';
	                		                        else if( permission[5] == 'w' )
    	                    		                    capability_list[i1].list[pos1].permission = 'w';
        	                                        else
            	                                        capability_list[i1].list.erase( capability_list[i].list.begin() + pos1 );

	                                		    }
											}
											else
											{
                                                if( capability_list[i1].group == group && !capability_list[i1].myself( username ) )
                                                {   
                                                    if( permission[2] == 'r' && permission[3] == 'w' )
                                                        capability_list[i1].push( filename, 'a');
                                                    else if( permission[2] == 'r' )
                                                        capability_list[i1].push( filename, 'r');
                                                    else if( permission[3] == 'w' )
                                                        capability_list[i1].push( filename, 'w');
                                                }
                                                if( capability_list[i1].group != group )
                                                {
                                                    if( permission[4] == 'r' && permission[5] == 'w' )
                                                        capability_list[i1].push( filename, 'a');
                                                    else if( permission[4] == 'r' )
                                                        capability_list[i1].push( filename, 'r');
                                                    else if( permission[5] == 'w' )
														capability_list[i1].push( filename, 'w');
                                                }
											}
										}
									}
									break;
								}
							}
/*							if( strncmp( username, owner, strlen( username ) ) == 0 )
							{ 
								access = 0;
								current_file = filename;
								while( !access )
								{
									pthread_mutex_lock( &lock_operated_file );
									if( files_status[current_file].status == '-' || files_status[current_file].status == 0 )
									{
										files_status[current_file].status = 'c';
										pthread_mutex_unlock( &lock_operated_file );

										memset( buffer_write, 0, MAX_BUFFER_LEN );
										sprintf( buffer_write, "srv/.%s", filename );
					                    pthread_mutex_lock( &lock_fopen );
										if( ( current_operated_file = fopen( buffer_write, "w" ) ) == 0 )
										{
			                                fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
            			                    perror("");
                        			        exit( 1 );
                           				}
										fprintf( current_operated_file, "%s %s %d\n", permission, username, group );
										fclose( current_operated_file );
					                    pthread_mutex_unlock( &lock_fopen );

										access = 1;
										pthread_mutex_lock( &lock_operated_file );
										files_status[current_file].status = '-';
										pthread_mutex_unlock( &lock_operated_file );
									}
									else
									{
										pthread_mutex_unlock( &lock_operated_file );
										sleep( 1 );
									}
								}

								memset( buffer_write, 0, MAX_BUFFER_LEN );
								sprintf( buffer_write + 3, "cat srv/.%s ", filename );
								write_all( fd, buffer_write );
							}
							else
							{
								memset( buffer_write, 0, MAX_BUFFER_LEN );
    	                        sprintf( buffer_write + 3, "echo \" [C] You did not get the permission ! \"" );
	                            write_all( fd, buffer_write );
							}*/
						}
						else
	                    {
    	                    memset( buffer_write, 0, MAX_BUFFER_LEN );
        	                sprintf( buffer_write + 3, "echo [C] Wrong command " );
            	            write_all( fd, buffer_write );
                	    }
					}
					else
					{
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write + 3, "echo [C] Wrong command " );
						write_all( fd, buffer_write );
					}
				break;
				//information
				case 4:
					temp = strtok( NULL, " \t\n\0" );					
					if( temp != NULL )
					{
						memset( filename, 0, MAX_FILENAME_LEN );
						strncpy( filename, temp, MAX_FILENAME_LEN );

						memset( permission, 0, MAX_PERMISSION_LEN );
						memset( owner, 0, MAX_USERNAME_LEN );
						memset( buffer_write, 0, MAX_BUFFER_LEN );
						sprintf( buffer_write, "srv/.%s", filename );
	                    pthread_mutex_lock( &lock_fopen );
						if( ( current_operated_file = fopen( buffer_write, "r" ) ) == 0 )
						{
							fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
	                        perror("");
    	                    exit( 1 );
						}

						fscanf( current_operated_file, "%s %s %d", permission, owner, &file_group );
						fclose( current_operated_file );
	                    pthread_mutex_unlock( &lock_fopen );

						memset( buffer_write, 0, MAX_BUFFER_LEN );
						memset( buffer_read, 0, MAX_BUFFER_LEN );

						switch( file_group )
						{
						case 0:
							sprintf( buffer_read, "AOS_students" );
							break;
						case 1:
                            sprintf( buffer_read, "CSE_students" );
                            break;
						default:
                            sprintf( buffer_read, "other_students" );
						}

						sprintf( buffer_write + 3, "echo %s %s %s `ls -l srv/%s | cut -d\" \"  -f1-4,10 --complement` %s ", permission, owner, buffer_read, filename, filename );
						write_all( fd, buffer_write );
					}
					else
					{
                        memset( buffer_write, 0, MAX_BUFFER_LEN );
                        sprintf( buffer_write + 3, "echo [C] Wrong command " );
                        write_all( fd, buffer_write );
                    }
				break;
				case 5:
					close( fd );
					cout << " [S] User \"" << username << "\" left from the system " << endl;
					pthread_exit( NULL );
				break;
			default :
				cout << " [S] Unknown command " << endl;
			}
		}
		
	}
}

int main()
{/*
	FILE *x;
	x = popen("ls srv", "r");

	char ab[2048];
	memset( ab,0,2048);
	while( fscanf( x, "%s", ab) == 1 )
		puts(ab);
	fclose( x );
	exit(0);*/
	system("clear");
	// for user to select the execution role
	int option;

	cout << " * Welcome to the System " << endl
		 << " * please select your role " << endl
		 << " * [0] for SERVER " << endl
		 << " * [1] for CLIENT " << endl
		 << " > ";

	cin >> option;
	
	uint16_t srv_port;
	string srv_ip;

	pthread_mutex_init( &lock_fopen, NULL );
	pthread_mutex_init( &lock_user_list, NULL );
	pthread_mutex_init( &lock_operated_file, NULL );
	switch( option )
	{
	case 0:
		cout << " * Please enter the listenning port : ";
		cin >> srv_port;
		run_srv( srv_port );
		break;
	case 1:
		cout << " * Please enter the Server's IP : ";
		cin >> srv_ip;
		cout << " * Please enter the Server's port : ";
		cin >> srv_port;
		run_cli( srv_ip, srv_port );
		break;
	default:
		cout << " * Please enter 0 or 1 " << endl;
	}

	return 0;
}

void run_cli( string srv_ip, uint16_t srv_port )
{
	cout << " [C] Run as Client " << endl;
	int fd;
	puts("create socket");
	//create socket
	if( ( fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == -1 )
	{
		fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
		perror("");
		exit( 1 );
	}
	
	struct sockaddr_in srv;
	srv.sin_family = AF_INET;
	srv.sin_port = htons( srv_port );
	srv.sin_addr.s_addr = inet_addr( srv_ip.c_str() );

	puts("connect");
	if( connect( fd, ( struct sockaddr* ) &srv, sizeof( srv ) ) < 0 )
	{
		fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
		perror("");
		exit( 1 );
	}

	char buffer_write[MAX_BUFFER_LEN], buffer_read[MAX_BUFFER_LEN], username[MAX_USERNAME_LEN];

	getchar();

	cout << " [C] Username : ";
	fgets( username, MAX_USERNAME_LEN - 1, stdin );
	
	sprintf( buffer_write + 3, "%s", username );
	write_all( fd, buffer_write );

	int group;
	read_all( fd, buffer_read );
	if( strncmp( buffer_read + 3, "new", 3 ) == 0 )
	{
        cout << " [C] Please select your group : " << endl
             << "   0 : AOS_students " << endl
             << "   1 : CSE_students " << endl
             << "   2 : other_students " << endl;
        cin >> group;
		getchar();

		memset( buffer_write, 0, MAX_BUFFER_LEN );
		sprintf( buffer_write + 3, "%d", group );
		write_all( fd, buffer_write );
	}
	else
	{
		sscanf( buffer_read, "%d", &group );
	}
	
	switch( group )
	{
	case 0:
		cout << " [C] you are at the group : AOS_students " << endl;
		break;
	case 1:
		cout << " [C] you are at the group : CSE_students " << endl;
		break;
	case 2:
		cout << " [C] you are at the group : other_students " << endl;
		break;
	}

	cout << " > ";
	while( fgets( buffer_write + 3, MAX_BUFFER_LEN - 1, stdin ) )
	{
		write_all( fd, buffer_write );
		if( strncmp( buffer_write + 3, "bye", 3 ) == 0 )
			break;
		memset( buffer_write, 0, MAX_BUFFER_LEN );

		read_all( fd, buffer_read );
		system( buffer_read + 3 );
		cout << " > ";
	}

	close( fd );
	cout << " [C] Bye, " << username << endl;
}

void write_all( int fd, char buffer_write[] )
{
	int msg_len, writen_len, current_writen_len;

	( buffer_write + 3 )[strlen( buffer_write + 3 ) - 1] = '\0';
    msg_len = strlen( buffer_write + 3 );
    sprintf( buffer_write, "%2x", msg_len );
    msg_len += 3;

    writen_len = 0;
    while( ( current_writen_len = write( fd, buffer_write + writen_len, msg_len - writen_len ) ) < msg_len - writen_len )
    {
        writen_len += current_writen_len;
    }

}

int read_all( int fd, char buffer_read[] )
{
	memset( buffer_read, 0, MAX_BUFFER_LEN );
	int total_receive_len, msg_len, current_receive_len;
	if( ( total_receive_len = read( fd, buffer_read, MAX_BUFFER_LEN ) ) != 0 )
    {
        sscanf( buffer_read, "%2x", &msg_len );
        while( total_receive_len < msg_len + 2 )
        {
            current_receive_len = read( fd, buffer_read + total_receive_len, MAX_BUFFER_LEN - total_receive_len );
            total_receive_len += current_receive_len;
        }
        // cout << msg_len << " " << buffer_read << endl << buffer_read + 3<< endl;
		return 1;
    }
	else
		return 0;

}

int identify_command( char *command )
{
	if( strncmp( command, "new", 3 ) == 0 )
		return 0;
	else if( strncmp( command, "read", 4 ) == 0 )
		return 1;
	else if( strncmp( command, "write", 5 ) == 0 )
        return 2;
	else if( strncmp( command, "change", 6 ) == 0 )
        return 3;
	else if( strncmp( command, "information", 11 ) == 0 )
        return 4;
	else if( strncmp( command, "bye", 3 ) == 0 )
		return 5;
	else
		return -1;
}

void run_srv( uint16_t srv_port )
{
	for( int i = 0 ; i < MAX_CLIENT_NUM ; i++ )
		capability_list[i].group = -1;
	system( "clear" );
	FILE *file, *file1;
	if( ( file = fopen( "srv/user_list", "r" ) ) == 0 )
	{
	    fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

	char buffer[MAX_BUFFER_LEN], buffer1[MAX_BUFFER_LEN], buffer2[MAX_BUFFER_LEN];
	int temp_i;
	usernum = 0;

	memset( buffer, 0, MAX_BUFFER_LEN );

	while( fscanf( file, "%d %s", &temp_i, buffer ) == 2 )
	{
		capability_list[usernum++].set( buffer, temp_i );
		memset( buffer, 0, MAX_BUFFER_LEN );
	}
	fclose( file );

	if( ( ( file = popen( "cd srv;ls .[^.]*", "r" ) ) == 0 ) || ( ( file1 = popen( "cat `ls srv/.[^.]*`", "r" ) ) == 0 ) )
	{
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

	memset( buffer, 0, MAX_BUFFER_LEN );
	char temp_c;
	while( fscanf( file, "%s", buffer ) == 1 )
	{
		puts(buffer);
		memset( buffer1, 0, MAX_BUFFER_LEN );memset( buffer2, 0, MAX_BUFFER_LEN );
		fscanf( file1, "%s %s %d", buffer1, buffer2, &temp_i );
		cout << buffer1 << " " << buffer2 << " " << temp_i << endl;
		temp_c = 0;
		for( int i = 0 ; i < usernum ; i++ )
		{
			if( strncmp( capability_list[i].username, buffer2, strlen( buffer2 ) ) == 0 )
            {
                if( buffer1[0] == 'r' && buffer1[1] == 'w' )
                    capability_list[i].push( buffer + 1, 'a' );
                else if( buffer1[0] == 'r' )
                    capability_list[i].push( buffer + 1, 'r');
                else if( buffer1[1] == 'w' )
                    capability_list[i].push( buffer + 1, 'w');
				else
                    capability_list[i].push( buffer + 1, 'c');
            }
			if( capability_list[i].group == temp_i && ( strncmp( capability_list[i].username, buffer2, strlen( buffer2 ) ) != 0 ) )
	        {
    	        if( buffer1[2] == 'r' && buffer1[3] == 'w' )
                    capability_list[i].push( buffer + 1, 'a');
                else if( buffer1[2] == 'r' )
                    capability_list[i].push( buffer + 1, 'r');
              	else if( buffer1[3] == 'w' )
                    capability_list[i].push( buffer + 1, 'w');
			}
			if( capability_list[i].group != temp_i )
			{
                if( buffer1[4] == 'r' && buffer1[5] == 'w' )
                    capability_list[i].push( buffer + 1, 'a');
                else if( buffer1[4] == 'r' )
                    capability_list[i].push( buffer + 1, 'r');
                else if( buffer1[5] == 'w' )
                    capability_list[i].push( buffer + 1, 'w');
			}
		}
		memset( buffer, 0, MAX_BUFFER_LEN );
	}
	fclose( file );
	fclose( file1 );
    cout << " [B] Run as Server " << endl;

    //create listenning socket
    struct sockaddr_in srv, cli;
    int listen_fd;
    socklen_t cli_len = sizeof( cli );

    if( ( listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == -1 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl( INADDR_ANY );
    srv.sin_port = htons( srv_port );
	puts(" [B] bind");
    //bind
    if( bind( listen_fd, ( struct sockaddr* ) &srv, sizeof( srv ) ) < 0 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }
	puts(" [B] listen");
    //listen
    if( listen( listen_fd, 5 ) == -1 )
    {
        fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
        perror("");
        exit( 1 );
    }

    int connect_fd;
	uint32_t current_clients_num = 0;
	pthread_t threadId_of_cli[MAX_CLIENT_NUM];

    while( 1 )
    {
		puts(" [B] accept");
        //accept
        if( ( connect_fd = accept( listen_fd, ( struct sockaddr* ) &cli, &cli_len ) ) == -1 )
        {
            fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
            perror("");
            exit( 1 );
        }

		puts(" [B] create threads");
		//create threads
		if( ( pthread_create( &threadId_of_cli[current_clients_num], NULL, srv_to_cli, ( void * )&connect_fd ) ) != 0 )
		{
			fprintf( stderr, "\n [ERR] %s() : line_%d : ", __FUNCTION__, __LINE__ - 2 );
			perror("");
			exit( 1 );
		}
		
		sleep( 1 );
    }

	close( listen_fd );
}

