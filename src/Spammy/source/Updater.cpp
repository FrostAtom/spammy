#include "Updater.h"
#include "Apps.h"
#include <cstdio>
#include <winhttp.h>

static constexpr const wchar_t* UPDATER_API_HOST = L"api.github.com";
static constexpr const wchar_t* UPDATER_API_PATH = L"/repos/FrostAtom/spammy/releases/latest";
static constexpr const wchar_t* UPDATER_FALLBACK_URL = L"https://github.com/FrostAtom/spammy/releases/latest";
static constexpr std::chrono::days UPDATER_BUILD_TO_PUBLISH_LAG{2};

static constexpr char s_months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

Updater& Updater::Instance()
{
    static Updater s_updater;
    return s_updater;
}

static std::optional<std::chrono::sys_days> MakeDate(int year, int month, int day)
{
    const std::chrono::year_month_day date{std::chrono::year(year), std::chrono::month((unsigned)month),
                                           std::chrono::day((unsigned)day)};
    if (!date.ok()) return std::nullopt;
    return std::chrono::sys_days(date);
}

static std::optional<std::chrono::sys_days> ParseBuildDate()
{
    char month[4] = {0};
    int day = 0;
    int year = 0;
    if (sscanf_s(__DATE__, "%3s %d %d", month, (unsigned)sizeof(month), &day, &year) != 3) return std::nullopt;
    const char* found = strstr(s_months, month);
    if (!found) return std::nullopt;
    return MakeDate(year, (int)(found - s_months) / 3 + 1, day);
}

static std::string HttpGet(const wchar_t* host, const wchar_t* path)
{
    std::string result;
    HINTERNET session = WinHttpOpen(L"" APP_NAME, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return result;
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available) {
                const size_t offset = result.size();
                result.resize(offset + available);
                DWORD read = 0;
                if (!WinHttpReadData(request, result.data() + offset, available, &read)) {
                    result.clear();
                    break;
                }
                result.resize(offset + read);
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

void Updater::CheckAsync(std::function<void()> onUpdateFound)
{
    if (std::exchange(_started, true)) return;
    std::thread([result = _result, onUpdateFound = std::move(onUpdateFound)] {
        const auto build = ParseBuildDate();
        if (!build) return;
        const std::string body = HttpGet(UPDATER_API_HOST, UPDATER_API_PATH);
        if (body.empty()) return;
        constexpr bool kThrowOnMalformedJson = false;
        const nlohmann::json json = nlohmann::json::parse(body, nullptr, kThrowOnMalformedJson);
        const auto publishedAt = json.find("published_at");
        if (publishedAt == json.end() || !publishedAt->is_string()) return;
        int year = 0;
        int month = 0;
        int day = 0;
        if (sscanf_s(publishedAt->get_ref<const std::string&>().c_str(), "%d-%d-%d", &year, &month, &day) != 3) return;
        const auto release = MakeDate(year, month, day);
        if (!release) return;
        const bool releaseIsThisBuildOrOlder = *release - *build < UPDATER_BUILD_TO_PUBLISH_LAG;
        if (releaseIsThisBuildOrOlder) return;

        const std::string_view monthName = std::string_view(s_months).substr((size_t)(month - 1) * 3, 3);
        result->latestDate = fmt::format("{} {} {}", monthName, day, year);
        if (const auto url = json.find("html_url"); url != json.end() && url->is_string()) {
            const auto& u = url->get_ref<const std::string&>();
            result->releaseUrl.assign(u.begin(), u.end());
        } else {
            result->releaseUrl = UPDATER_FALLBACK_URL;
        }
        result->updateAvailable = true;
        onUpdateFound();
    }).detach();
}

void Updater::OpenReleasePage()
{
    if (_result->updateAvailable) LaunchUrl(_result->releaseUrl.c_str());
}
