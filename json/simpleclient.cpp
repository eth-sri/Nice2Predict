/**
 * @file simpleclient.cpp
 * @date 03.01.2013
 * @author Peter Spiess-Knafl <peter.knafl@gmail.com>
 * @brief This is a simple client example.
 */

#include "json/client.h"
#include "json/client_connectors_httpclient.h"
#include <iostream>

using namespace jsonrpc;
using namespace std;

int main()
{
    HttpClient client("http://localhost:8383");
    Client c(client);

    Json::Value params;
    params["name"] = "Peter";

    try
    {
        cout << c.CallMethod("sayHello", params) << endl;
    }
    catch (JsonRpcException& e)
    {
        cerr << e.what() << endl;
    }


}
