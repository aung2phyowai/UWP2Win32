#include "stdafx.h"

class security_attributes
{
    SECURITY_ATTRIBUTES _securityAttributes;
    PSID pEveryoneSID;
    PACL pACL;
    PSECURITY_DESCRIPTOR pSD;
public:
    security_attributes(DWORD permissions) : pEveryoneSID(nullptr), pACL(nullptr), pSD(nullptr)
    {
        SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

        // Create a well-known SID for the Everyone group.
        if (!AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSID))
        {
            printf("AllocateAndInitializeSid Error %u\n", GetLastError());
            return;
        }

        EXPLICIT_ACCESS ea = { 0 };
        ea.grfAccessPermissions = permissions;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;

        // Create a new ACL that contains the new ACEs.
        DWORD dwRes = SetEntriesInAclW(1, &ea, NULL, &pACL);
        if (dwRes != ERROR_SUCCESS)
        {
            printf("SetEntriesInAcl Error %u\n", GetLastError());
            return;
        }

        // Initialize a security descriptor.  
        pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
        if (pSD == NULL)
        {
            printf("LocalAlloc Error %u\n", GetLastError());
            return;
        }

        if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
        {
            printf("InitializeSecurityDescriptor Error %u\n", GetLastError());
            return;
        }

        // Add the ACL to the security descriptor. 
        if (!SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE))
        {
            printf("SetSecurityDescriptorDacl Error %u\n", GetLastError());
            return;
        }

        // Initialize a security attributes structure.
        _securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        _securityAttributes.lpSecurityDescriptor = pSD;
        _securityAttributes.bInheritHandle = FALSE;
    }

    ~security_attributes()
    {
        if (pEveryoneSID)
            FreeSid(pEveryoneSID);
        if (pACL)
            LocalFree(pACL);
        if (pSD)
            LocalFree(pSD);
    }

    LPSECURITY_ATTRIBUTES get_sa()
    {
        return &_securityAttributes;
    }
};

// HACK: this gives all access to anyone, allows client impersonation
void init_security_attributes2(LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    auto pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);

    if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION))
    {
        printf("InitializeSecurityDescriptor Error %d\n", GetLastError());
        return;
    }

    if (!SetSecurityDescriptorDacl(pSD, TRUE, NULL, FALSE))
    {
        printf("SetSecurityDescriptorDacl Error %d\n", GetLastError());
        return;
    }

    lpSecurityAttributes->nLength = sizeof(SECURITY_ATTRIBUTES);
    lpSecurityAttributes->lpSecurityDescriptor = pSD;
    lpSecurityAttributes->bInheritHandle = FALSE;
}

int main()
{
    HANDLE hPipe;
    char buffer[1024];
    DWORD dwRead;

    security_attributes sa(GENERIC_WRITE | GENERIC_READ);

    hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\ztp-client-pipe"),
        PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
        PIPE_WAIT,
        1,
        1024 * 16,
        1024 * 16,
        NMPWAIT_USE_DEFAULT_WAIT,
        sa.get_sa());
    if (hPipe != INVALID_HANDLE_VALUE)
    {
        while (true)
        {
            printf("Waiting for someone to connect...\n");
            if (ConnectNamedPipe(hPipe, NULL) != FALSE)
            {
                printf("Reading data...\n");
                //while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
                BOOL readResult = ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL);
                if(readResult)
                {
                    /* add terminating zero */
                    buffer[dwRead] = '\0';

                    /* do something with data in buffer */
                    printf("Received '%s'\n", buffer);

                    DWORD dwWritten;
                    for (int i = 0; i < (int)dwRead; ++i)
                    {
                        buffer[i] = toupper(buffer[i]);
                    }

                    BOOL writeResult = WriteFile(hPipe,
                        buffer,
                        strlen(buffer),
                        &dwWritten,
                        NULL);

                    printf("Response '%s' written\n", buffer);

                    if (!writeResult) {
                        printf("WriteFile Error %d\n", GetLastError());
                    }

                }
                else {
                    printf("ReadFile Error %d\n", GetLastError());
                }
            }
            else
            {
                printf("ConnectNamedPipe Error %d\n", GetLastError());
            }
            DisconnectNamedPipe(hPipe);
        }
    }
    else
    {
        printf("CreateNamedPipe Error %d\n", GetLastError());
    }

    return 0;
}


