// WinHttpClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include "winhttp.h"

#include <stdio.h>


/*

TNetHTTPClient & TNetHTTPRequest

https://stackoverflow.com/questions/13690721/how-to-reuse-the-same-https-connection-for-many-put-requests-within-a-session

https://security.stackexchange.com/questions/179352/winhttp-prevent-successful-handshake-if-peer-certificate-is-invalid


hSession = WinHttpOpen( L"Agent/1.0",..
hConnect = WinHttpConnect(hSession,..
for (all_files_to_upload) {
	hRequest = WinHttpOpenRequest( hConnect, L"PUT",..
	WinHttpSetCredentials(hRequest,..
	WinHttpAddRequestHeaders( hRequest,..
	WinHttpSendRequest( hRequest,..
	WinHttpWriteData(hRequest,..
	WinHttpReceiveResponse(hRequest,..
	WinHttpQueryHeaders(hRequest,..
	WinHttpCloseHandle(hRequest);
}

if (hConnect) WinHttpCloseHandle(hConnect);
if (hSession) WinHttpCloseHandle(hSession);

*/

void CALLBACK http_callback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

int _tmain(int argc, _TCHAR* argv[])
{
	//https://192.168.1.100:8443/acsline/ver

	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	DWORD dwError = 0;
	LPSTR pszOutBuffer;
	BOOL  bResults = FALSE;
	HINTERNET  hSession = NULL,
	hConnect = NULL,
	hRequest = NULL;	

	// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen(L"WinHTTP Example/1.0",
							WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
							WINHTTP_NO_PROXY_NAME,
							WINHTTP_NO_PROXY_BYPASS, 0);
	
	if (hSession == NULL)
	{
		dwError = GetLastError();
		wprintf(L"WinHttpOpen failed with error %d\n", dwError);
		goto Exit;
	}

	if (!WinHttpSetTimeouts(hSession, 0, 60000, 60000, 60000))
	{
		dwError = GetLastError();
		wprintf(L"WinHttpSetTimeouts failed with error %d\n", dwError);
		goto Exit;
	}

	DWORD supportedProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | // TLS 1.0 is support for back-compat reasons (https://docs.microsoft.com/en-us/azure/iot-fundamentals/iot-security-deployment)
		WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
		WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

	if (!WinHttpSetOption(hSession,
							WINHTTP_OPTION_SECURE_PROTOCOLS,
							&supportedProtocols,
							sizeof(supportedProtocols)))
	{
		dwError = GetLastError();
			wprintf(L"WinHttpSetOption failed with error %d\n", dwError);
			goto Exit;
	}

	WinHttpSetStatusCallback(hSession, (WINHTTP_STATUS_CALLBACK)http_callback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	// Specify an HTTP server.
	hConnect = WinHttpConnect(hSession, L"192.168.1.100",
		/*INTERNET_DEFAULT_HTTPS_PORT*/ 8443, 0);

	if (hConnect == NULL)
	{
		dwError = GetLastError();
		wprintf(L"WinHttpConnect failed with error %d\n", dwError);
		goto Exit;
	}

	for (int i = 0; i < 5; i++)
	{
		// Create an HTTP request handle.
		hRequest = WinHttpOpenRequest(hConnect,
			L"GET",
			L"/acsline/ver",
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_REFRESH | WINHTTP_FLAG_SECURE);

		if (hRequest == NULL)
		{
			dwError = GetLastError();
			wprintf(L"WinHttpOpenRequest failed with error %d\n", dwError);
			goto Exit;
		}
#if 1
		bResults = WinHttpSetOption(hRequest,
			WINHTTP_OPTION_CLIENT_CERT_CONTEXT,
			WINHTTP_NO_CLIENT_CERT_CONTEXT,
			0);

		if (!bResults)
		{
			dwError = GetLastError();
			wprintf(L"WinHttpSetOption failed with error %d\n", dwError);
			goto Exit;
		}		
#endif
		/*SECURITY_FLAG_IGNORE_UNKNOWN_CA |
		SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
		SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
		SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;*/
		DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;					
		bResults = WinHttpSetOption(hRequest,
							WINHTTP_OPTION_SECURITY_FLAGS,
							&dwFlags,
							sizeof(dwFlags));

		if (!bResults)
		{
			dwError = GetLastError();
			wprintf(L"WinHttpSetOption failed with error %d\n", dwError);
			goto Exit;
		}
#if 0
		// Use WinHttpSetCredentials with NULL username and password
		// to use default credentials
		if (!WinHttpSetCredentials(hRequest,
			WINHTTP_AUTH_TARGET_SERVER,
			WINHTTP_AUTH_SCHEME_NTLM,
			NULL,
			NULL,
			NULL))
		{
			dwError = GetLastError();
			wprintf(L"WinHttpSetCredentials failed with error %d\n", dwError);
			goto Exit;
		}
#endif

		//send
		bResults = WinHttpSendRequest(hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0,
			0, 0);
		if (!bResults)
		{
			dwError = GetLastError();
			wprintf(L"WinHttpSendRequest failed with error %d\n", dwError);
			goto Exit;
		}

		//receive
		bResults = WinHttpReceiveResponse(hRequest, NULL);
		if (!bResults)
		{
			dwError = GetLastError();
			wprintf(L"WinHttpReceiveResponse failed with error %d\n", dwError);
			goto Exit;
		}

		// check status
		DWORD dwStatus;
		DWORD dwLength = sizeof(dwStatus);

		bResults = WinHttpQueryHeaders(hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			LPCWSTR(&dwStatus),
			&dwStatus,
			&dwLength,
			NULL);

		if (!bResults)
		{
			printf("can't query status %d\n", GetLastError());
		}
		else
		{
			printf("Status: %d\n", dwStatus);
		}

		// Keep checking for data until there is nothing left.
		do
		{
			// Check for available data.
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				printf("Error %u in WinHttpQueryDataAvailable.\n",
					GetLastError());

			if (dwSize == 0)
				break;

			// Allocate space for the buffer.
			pszOutBuffer = new char[dwSize + 1];
			if (!pszOutBuffer)
			{
				printf("Out of memory\n");
				dwSize = 0;
			}
			else
			{
				// Read the data.
				ZeroMemory(pszOutBuffer, dwSize + 1);

				if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
					dwSize, &dwDownloaded))
					printf("Error %u in WinHttpReadData.\n", GetLastError());
				else
				{					
					printf("Size: %d\n", dwDownloaded);
				}

				// Free the memory allocated to the buffer.
				delete[] pszOutBuffer;
			}
		} while (dwSize > 0);

		if (hRequest)
		{
			WinHttpCloseHandle(hRequest);
			hRequest = NULL;
		}
	}

Exit:

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	return dwError;
}

/*

WINHTTP_CALLBACK_STATUS_HANDLE_CREATED
WINHTTP_CALLBACK_STATUS_HANDLE_CREATED
WINHTTP_CALLBACK_STATUS_RESOLVING_NAME
WINHTTP_CALLBACK_STATUS_NAME_RESOLVED
WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER
WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER
WINHTTP_CALLBACK_STATUS_SENDING_REQUEST
WINHTTP_CALLBACK_STATUS_REQUEST_SENT
WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE
WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED
Status: 200
Size: 71
WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION
WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED
WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING

*/

void CALLBACK http_callback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength) {
	switch (dwInternetStatus) {
	case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
		printf("WINHTTP_CALLBACK_STATUS_RESOLVING_NAME\n");
		break;
	case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
		printf("WINHTTP_CALLBACK_STATUS_NAME_RESOLVED\n");
		break;
	case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
		printf("WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER\n");
		break;
	case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
		printf("WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER\n");
		break;
	case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
		printf("WINHTTP_CALLBACK_STATUS_SENDING_REQUEST\n");

		//WINHTTP_OPTION_SERVER_CERT_CONTEXT
		{
			PCERT_CONTEXT pCertContext = NULL;
			DWORD bufferLength = sizeof(pCertContext);
			bool certificateTrusted;
			BOOL  bResults = FALSE;
			bResults = WinHttpQueryOption(hInternet, WINHTTP_OPTION_SERVER_CERT_CONTEXT, (void*)&pCertContext, &bufferLength);
			if (!bResults)
			{
				DWORD dwError = GetLastError();
				wprintf(L"WinHttpQueryOption failed with error %d\n", dwError);
			}

			if (pCertContext != NULL)
			{
				WinHttpCloseHandle(pCertContext);
			}
		}
		break;
	case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
		printf("WINHTTP_CALLBACK_STATUS_REQUEST_SENT\n");
		break;
	case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
		printf("WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
		printf("WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED\n");
		break;
	case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
		printf("WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION\n");
		break;
	case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
		printf("WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED\n");
		break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
		printf("WINHTTP_CALLBACK_STATUS_HANDLE_CREATED\n");
		break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		printf("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING\n");
		break;
	case WINHTTP_CALLBACK_STATUS_DETECTING_PROXY:
		printf("WINHTTP_CALLBACK_STATUS_DETECTING_PROXY\n");
		break;
	case WINHTTP_CALLBACK_STATUS_REDIRECT:
		printf("WINHTTP_CALLBACK_STATUS_REDIRECT\n");
		break;
	case WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE:
		printf("WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
		printf("WINHTTP_CALLBACK_STATUS_SECURE_FAILURE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		printf("WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
		printf("WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		printf("WINHTTP_CALLBACK_STATUS_READ_COMPLETE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
		printf("WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		printf("WINHTTP_CALLBACK_STATUS_REQUEST_ERROR\n");
		break;
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		printf("WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE:
		printf("WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_CLOSE_COMPLETE:
		printf("WINHTTP_CALLBACK_STATUS_CLOSE_COMPLETE\n");
		break;
	case WINHTTP_CALLBACK_STATUS_SHUTDOWN_COMPLETE:
		printf("WINHTTP_CALLBACK_STATUS_SHUTDOWN_COMPLETE\n");
		break;
	default:
		printf("Internet status is %u\n", dwInternetStatus);
		break;
	}
}