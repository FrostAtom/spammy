#include "Updater.h"
#include "Utils.h"
#include <chrono>
#define UPDATER_API_HOST L"api.github.com"
#define UPDATER_API_PATH L"/repos/FrostAtom/spammy/releases/latest"
#define UPDATER_FALLBACK_URL L"https://github.com/FrostAtom/spammy/releases/latest"
#define UPDATER_PUBLISH_SLACK_DAYS 2

Updater& Updater::Instance()
{
    static std::unique_ptr<Updater> updater(new Updater());
    return *updater;
}

static const char s_months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

static bool MakeDate(int year, int month, int day, std::chrono::sys_days& out)
{
    std::chrono::year_month_day date{std::chrono::year(year), std::chrono::month((unsigned)month),
                                     std::chrono::day((unsigned)day)};
    if (!date.ok()) return false;
    out = std::chrono::sys_days(date);
    return true;
}

static bool ParseBuildDate(std::chrono::sys_days& out)
{
    char month[4] = {0};
    int day = 0, year = 0;
    if (sscanf_s(__DATE__, "%3s %d %d", month, (unsigned)sizeof(month), &day, &year) != 3) return false;
    const char* found = strstr(s_months, month);
    if (!found) return false;
    return MakeDate(year, (int)(found - s_months) / 3 + 1, day, out);
}

static std::string HttpGet(const wchar_t* host, const wchar_t* path)
{
    std::string result;
    HINTERNET session = WinHttpOpen(L"" APP_NAME, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return result;
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : NULL;
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, NULL)) {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available) {
                size_t offset = result.size();
                result.resize(offset + available);
                DWORD readed = 0;
                if (!WinHttpReadData(request, result.data() + offset, available, &readed)) {
                    result.clear();
                    break;
                }
                result.resize(offset + readed);
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

void Updater::CheckAsync()
{
    if (_thread.joinable()) return;
    _thread = std::jthread([this] {
        std::chrono::sys_days build;
        if (!ParseBuildDate(build)) return;
        std::string body = HttpGet(UPDATER_API_HOST, UPDATER_API_PATH);
        if (body.empty()) return;
        try {
            nlohmann::json json = nlohmann::json::parse(body);
            auto publishedAt = json["published_at"];
            if (!publishedAt.is_string()) return;
            const std::string& published = publishedAt.get_ref<const std::string&>();
            int year = 0, month = 0, day = 0;
            if (sscanf_s(published.c_str(), "%d-%d-%d", &year, &month, &day) != 3) return;
            std::chrono::sys_days release;
            if (!MakeDate(year, month, day, release)) return;
            if (release - build < std::chrono::days(UPDATER_PUBLISH_SLACK_DAYS)) return;

            char date[16];
            snprintf(date, sizeof(date), "%.3s %2d %d", s_months + (month - 1) * 3, day, year);
            _latestDate = date;
            if (auto url = json["html_url"]; url.is_string()) {
                const std::string& u = url.get_ref<const std::string&>();
                _releaseUrl.assign(u.begin(), u.end());
            } else {
                _releaseUrl = UPDATER_FALLBACK_URL;
            }
            _updateAvailable = true;
        } catch (nlohmann::json::exception&) {
        }
    });
}

bool Updater::IsUpdateAvailable()
{
    return _updateAvailable;
}

const char* Updater::LatestDate()
{
    return _latestDate.c_str();
}

void Updater::OpenReleasePage()
{
    if (_updateAvailable) LaunchUrl(_releaseUrl.c_str());
}
