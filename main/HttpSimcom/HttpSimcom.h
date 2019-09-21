#include <../DebugUtils/DebugUtils.h>
#include <../SerialModem/SerialModem.h>
#include <sstream>
#include <string>
#include <map>
#include <functional>
#include <regex>
#include <queue>
#include <memory>
#include <queue>

#define MAX_READ 200

class HttpSimcom
{
public:
    enum HttpStatusCode
    {
        Continue						=	100,
        Switching_Protocols				=	101,
        Processing						=	102,
        OK								=	200,
        Created							=	201,
        Accepted						=	202,
        Non_authoritative_Information	=	203,
        No_Content						=	204,
        Reset_Content					=	205,
        Partial_Content					=	206,
        Multi_Status					=	207,
        Already_Reported				=	208,
        IM_Used							=	226,
        Multiple_Choices				=	300,
        Moved_Permanently				=	301,
        Found							=	302,
        See_Other						=	303,
        Not_Modified					=	304,
        Use_Proxy						=	305,
        Temporary_Redirect				=	307,
        Permanent_Redirect				=	308,
        Bad_Request						=	400,
        Unauthorized					=	401,
        Payment_Required				=	402,
        Forbidden						=	403,
        Not_Found						=	404,
        Method_Not_Allowed				=	405,
        Not_Acceptable					=	406,
        Proxy_Authentication_Required	=	407,
        Request_Timeout					=	408,
        Conflict						=	409,
        Gone							=	410,
        Length_Required					=	411,
        Precondition_Failed				=	412,
        Payload_Too_Large				=	413,
        Request_URI_Too_Long			=	414,
        Unsupported_Media_Type			=	415,
        Requested_Range_Not_Satisfiab	=	416,
        Expectation_Failed				=	417,
        Im_a_teapot						=	418,
        Misdirected_Request				=	421,
        Unprocessable_Entity			=	422,
        Locked							=	423,
        Failed_Dependency				=	424,
        Upgrade_Required				=	426,
        Precondition_Required			=	428,
        Too_Many_Requests				=	429,
        Request_Header_Fields_Too_Lar	=	431,
        Connection_Closed_Without_Res	=	444,
        Unavailable_For_Legal_Reasons	=	451,
        Client_Closed_Request			=	499,
        Internal_Server_Error			=	500,
        Not_Implemented					=	501,
        Bad_Gateway						=	502,
        Service_Unavailable				=	503,
        Gateway_Timeout					=	504,
        HTTP_Version_Not_Supported		=	505,
        Variant_Also_Negotiates			=	506,
        Insufficient_Storage			=	507,
        Loop_Detected					=	508,
        Not_Extended					=	510,
        Network_Authentication_Requir	=	511,
        Network_Connect_Timeout_Error	=	599,

        Not_HTTP_Pdu = 600,
        No_Memory    = 602,
        DNS_Error    = 603,
        Stack_Busy   = 604,

        Init_Failed  = 700,
        Timeout      = 701
    };

    enum ActionHttp
    {
        Get = 0,
        Post = 1,
        Head = 2,
        Delete = 3
    };

    struct HttpRequest
    {
        std::string url;
        std::string data;
        ActionHttp action;
        std::map<std::string, std::string> header;
    };

    struct HttpResponse
    {
        const char *data;
        HttpStatusCode code;
        unsigned long timeTaken = 0;
        bool isGotReply()
        {
            return code > 599;
        }
    };

private:
    struct HttpQueue
    {
        unsigned int id;
        unsigned long timeStart;
        unsigned long timeEnd;
        HttpStatusCode status;
        char* p_dataOutput;
    };

public:
    HttpSimcom(SerialModem &serialModem)
    {
        m_serialModem = &serialModem;
    }
    void HttpDo(HttpRequest req, 
                std::function<void(HttpResponse &)> callbackSuccess,
                std::function<void(HttpResponse &)> callbackFail,
                unsigned int timeout = 60);
    bool InternetTest();

    ~HttpSimcom();

private:
    void ShiftStringLeft(char *string,int shiftLength)
    {

        int i = 0;
        int size=strlen(string);
        if(shiftLength >= size)
        {
            memset(string,0 ,size);
            return;
        }

        for (i=0; i < size-shiftLength; i++)
        {
            string[i] = string[i + shiftLength];
            string[i + shiftLength] = 0;
        }
    }
    unsigned GetNumberOfDigits (unsigned i)
    {
        return i > 0 ? (int) log10 ((double) i) + 1 : 1;
    }

    SerialModem           *m_serialModem;
    std::queue<HttpQueue> m_queue;
    unsigned int          m_counter;
};

