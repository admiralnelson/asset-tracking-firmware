#include "HttpSimcom.h"

void HttpSimcom::HttpDo(HttpRequest req, 
            std::function<void(HttpResponse &)> callbackSuccess,
            std::function<void(HttpResponse &)> callbackFail)
{
    //SerialModem::Command *c = new Command("")
    //m_serialModem->Enqueue();
}

bool HttpSimcom::InternetTest()
{
    return true;
}
