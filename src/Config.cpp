#include "Config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

static inline bool fileExists(std::string path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0); 
}


static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

Config* Config::getInstance()
{
    static Config config;
    return &config;
}
void Config::check_variable(int &var,std::string value,std::string name)
{
    var = stoi(value);
    printf("Loaded variable %s with value %d\n", name.c_str(), var);
}
void Config::check_variable(std::string &var, std::string value, std::string name)
{
    var = value;
    printf("Loaded variable %s with value %s\n", name.c_str(), value.c_str());
}
bool Config::parseCommandLine(int argc, char**argv)
{
	for(unsigned char c; (c=getopt(argc, argv, ":hfc:u:p:m:")) != -1;)
	{
		switch(c) {
			case 'h':
			case 'f':
				fuseindex = optind;
				return false;
			case 'c':
				break;
			case 'u':
				USERNAME = optarg;
				break;
			case 'p':
				PASSWORD = optarg;
				break;
			case 'm':
				MOUNTPOINT = optarg;
				break;
			case '?':
				if(isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				return true;
			case ':':
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
				return true;
			default:
				return false;
        }
    }
    if (optind < argc) {
        printf ("non-option ARGV-elements: ");
        while (optind < argc)
            printf ("%s ", argv[optind++]);
        printf ("\n");
    }
    /*printf("optind: %d, argc: %d\n", optind, argc);
    if(optind+1 == argc)
    {
    	printf("ooo: %s\n", optarg);
    } else if(optind < argc)
    {
        for(int index = optind; index < argc; index++)
            printf("Non-option argument %s\n", argv[index]);
        return true;
    }*/
    return false;
}
std::string Config::getString(std::string prompt, bool isPassword)
{
	std::string buffer;
	printf("%s", prompt.c_str());

	struct termios oldt, newt;
	if(isPassword)
	{
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ECHO);          
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}
	std::cin >> buffer;

	if(isPassword)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
		printf("\n");
	}
	return buffer;	
}
static int countEntriesInDir(std::string dirname)
{
        int n=0;
        dirent* d;
        DIR* dir = opendir(dirname.c_str());
        if (dir == NULL) return -1;
        while((d = readdir(dir))!=NULL) n++;
        closedir(dir);
        return n;
}
void Config::LoadConfig()
{
	std::ifstream file(configFile, std::ifstream::in);
	printf("Using config file from: %s\n", configFile.c_str());
	std::string user = "";
	std::string pass = "";
	std::string mount = "";
	std::string cache = "";
	if(configFile.empty())
	{
		fprintf(stderr, "Config file not found.\n");
	} else if(!file.is_open())
	{
		fprintf(stderr, "Couldn't open config file: %s\n", configFile.c_str());	
	} else {
		std::string line;
		unsigned int linenum = 0;
		std::string key;
		std::string value;
		while(std::getline(file, line))
		{
			linenum++;
			if(line.length() == 0 || line.at(0) == '#')
				continue;
			if(line[line.length() - 1] == '\r')
				line = line.substr(0, line.length() - 1);
			std::istringstream is_line(line);
			key = "";
			if(std::getline(is_line, key, '='))
			{
				value = "";
				std::getline(is_line, value);
				key = trim(key);
				value = trim(value);
				if(key == "USERNAME")
					user = value;
				else if(key == "PASSWORD")
					pass = value;
				else if(key == "MOUNTPOINT")
					mount = value;
				else if(key == "APPKEY")
					APPKEY = value;
				else if(key == "CACHEPATH")
					cache = value;

			} else {
				fprintf(stderr, "Could not parse line %d: %s\n", linenum, line.c_str());			
			}
		}
		file.close();
	}
	if(USERNAME.empty() && !user.empty())
		USERNAME = user;
	else
		while(USERNAME.empty())
			USERNAME = getString("Username (email): ", false);
	if(PASSWORD.empty() && !pass.empty())
		PASSWORD = pass;
	else
		while(PASSWORD.empty())
			PASSWORD = getString("Enter your password: ", true);
	if(MOUNTPOINT.empty() && !mount.empty())
		MOUNTPOINT = mount;
	if(CACHEPATH.empty() && !cache.empty())
		CACHEPATH = cache;
	while(countEntriesInDir(MOUNTPOINT) != 2)
		MOUNTPOINT = getString("Specify a valid mountpoint (an empty directory): ", false);
	while(0 > countEntriesInDir(CACHEPATH))
		CACHEPATH = getString("Specify a valid cache path (eg: /tmp): ",false);
}
Config::Config():APPKEY("MEGASDK"), fuseindex(-1)
{
	std::string tmp = "./megafuse.conf";	
	if(fileExists(tmp))
	{
		configFile = tmp;
		return;	
	}
	tmp = getenv("HOME");
	if(tmp.empty())
	{
		struct passwd* pwd = getpwuid(getuid());
		if(pwd)
			tmp = pwd->pw_dir;
	}
	if(tmp.empty())
		return;
	tmp.append("/.config/MegaFuse/");
	if(mkdir(tmp.c_str(), 0755) != 0 && errno != EEXIST)
		return;
	tmp.append("megafuse.conf");
	if(fileExists(tmp))
		configFile = tmp;
}
