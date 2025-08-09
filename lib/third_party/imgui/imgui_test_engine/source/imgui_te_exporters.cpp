// dear imgui test engine
// (result exporters)
// Read https://github.com/ocornut/imgui_test_engine/wiki/Exporting-Results

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "imgui_te_exporters.h"
#include "imgui_te_engine.h"
#include "imgui_te_internal.h"
#include "thirdparty/Str/Str.h"

//-------------------------------------------------------------------------
// [SECTION] FORWARD DECLARATIONS
//-------------------------------------------------------------------------

static void ImGuiTestEngine_ExportJUnitXml(ImGuiTestEngine* engine, const char* output_file);

//-------------------------------------------------------------------------
// [SECTION] TEST ENGINE EXPORTER FUNCTIONS
//-------------------------------------------------------------------------
// - ImGuiTestEngine_PrintResultSummary()
// - ImGuiTestEngine_Export()
// - ImGuiTestEngine_ExportEx()
// - ImGuiTestEngine_ExportJUnitXml()
//-------------------------------------------------------------------------

void ImGuiTestEngine_PrintResultSummary(ImGuiTestEngine* engine)
{
    ImGuiTestEngineResultSummary summary;
    ImGuiTestEngine_GetResultSummary(engine, &summary);

    if (summary.CountSuccess < summary.CountTested)
    {
        printf("\nFailing tests:\n");
        for (ImGuiTest* test : engine->TestsAll)
            if (test->Output.Status == ImGuiTestStatus_Error)
                printf("- %s\n", test->Name);
    }

    bool success = (summary.CountSuccess == summary.CountTested);
    ImOsConsoleSetTextColor(ImOsConsoleStream_StandardOutput, success ? ImOsConsoleTextColor_BrightGreen : ImOsConsoleTextColor_BrightRed);
    printf("\nTests Result: %s\n", success ? "OK" : "Errors");
    printf("(%d/%d tests passed)\n", summary.CountSuccess, summary.CountTested);
    if (summary.CountInQueue > 0)
        printf("(%d queued tests remaining)\n", summary.CountInQueue);
    ImOsConsoleSetTextColor(ImOsConsoleStream_StandardOutput, ImOsConsoleTextColor_White);
}

// This is mostly a copy of ImGuiTestEngine_PrintResultSummary with few additions.
static void ImGuiTestEngine_ExportResultSummary(ImGuiTestEngine* engine, FILE* fp, int indent_count, ImGuiTestGroup group)
{
    int count_tested = 0;
    int count_success = 0;

    for (ImGuiTest* test : engine->TestsAll)
    {
        if (test->Group != group)
            continue;
        if (test->Output.Status != ImGuiTestStatus_Unknown)
            count_tested++;
        if (test->Output.Status == ImGuiTestStatus_Success)
            count_success++;
    }

    Str64 indent_str;
    indent_str.reserve(indent_count + 1);
    memset(indent_str.c_str(), ' ', indent_count);
    indent_str[indent_count] = 0;
    const char* indent = indent_str.c_str();

    if (count_success < count_tested)
    {
        fprintf(fp, "\n%sFailing tests:\n", indent);
        for (ImGuiTest* test : engine->TestsAll)
        {
            if (test->Group != group)
                continue;
            if (test->Output.Status == ImGuiTestStatus_Error)
                fprintf(fp, "%s- %s\n", indent, test->Name);
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "%sTests Result: %s\n", indent, (count_success == count_tested) ? "OK" : "Errors");
    fprintf(fp, "%s(%d/%d tests passed)\n", indent, count_success, count_tested);
}

static bool ImGuiTestEngine_HasAnyLogLines(ImGuiTestLog* test_log, ImGuiTestVerboseLevel level)
{
    for (auto& line_info : test_log->LineInfo)
        if (line_info.Level <= level)
            return true;
    return false;
}

static void ImGuiTestEngine_PrintLogLines(FILE* fp, ImGuiTestLog* test_log, int indent, ImGuiTestVerboseLevel level)
{
    Str128 log_line;
    for (auto& line_info : test_log->LineInfo)
    {
        if (line_info.Level > level)
            continue;
        const char* line_start = test_log->Buffer.c_str() + line_info.LineOffset;
        const char* line_end = strstr(line_start, "\n"); // FIXME: Incorrect.
        log_line.set(line_start, line_end);
        ImStrXmlEscape(&log_line); // FIXME: Should not be here considering the function name.

        // Some users may want to disable indenting?
        fprintf(fp, "%*s%s\n", indent, "", log_line.c_str());
    }
}

// Export using settings stored in ImGuiTestEngineIO
// This is called by ImGuiTestEngine_CrashHandler().
void ImGuiTestEngine_Export(ImGuiTestEngine* engine)
{
    ImGuiTestEngineIO& io = engine->IO;
    ImGuiTestEngine_ExportEx(engine, io.ExportResultsFormat, io.ExportResultsFilename);
}

// Export using custom settings.
void ImGuiTestEngine_ExportEx(ImGuiTestEngine* engine, ImGuiTestEngineExportFormat format, const char* filename)
{
    if (format == ImGuiTestEngineExportFormat_None)
        return;
    IM_ASSERT(filename != nullptr);

    if (format == ImGuiTestEngineExportFormat_JUnitXml)
        ImGuiTestEngine_ExportJUnitXml(engine, filename);
    else
        IM_ASSERT(0);
}

void ImGuiTestEngine_ExportJUnitXml(ImGuiTestEngine* engine, const char* output_file)
{
    IM_ASSERT(engine != nullptr);
    IM_ASSERT(output_file != nullptr);

    FILE* fp = fopen(output_file, "w+b");
    if (fp == nullptr)
    {
        fprintf(stderr, "Writing '%s' failed.\n", output_file);
        return;
    }

    // Per-testsuite test statistics.
    struct
    {
        const char* Name     = nullptr;
        int         Tests    = 0;
        int         Failures = 0;
        int         Disabled = 0;
    } testsuites[ImGuiTestGroup_COUNT];
    testsuites[ImGuiTestGroup_Tests].Name = "tests";
    testsuites[ImGuiTestGroup_Perfs].Name = "perfs";

    for (int n = 0; n < engine->TestsAll.Size; n++)
    {
        ImGuiTest* test = engine->TestsAll[n];
        auto* stats = &testsuites[test->Group];
        stats->Tests += 1;
        if (test->Output.Status == ImGuiTestStatus_Error)
            stats->Failures += 1;
        else if (test->Output.Status == ImGuiTestStatus_Unknown)
            stats->Disabled += 1;
    }

    // Attributes for <testsuites> tag.
    const char* testsuites_name = "Dear ImGui";
    int testsuites_failures = 0;
    int testsuites_tests = 0;
    int testsuites_disabled = 0;
    float testsuites_time = (float)((double)(engine->BatchEndTime - engine->BatchStartTime) / 1000000.0);
    for (int testsuite_id = ImGuiTestGroup_Tests; testsuite_id < ImGuiTestGroup_COUNT; testsuite_id++)
    {
        testsuites_tests += testsuites[testsuite_id].Tests;
        testsuites_failures += testsuites[testsuite_id].Failures;
        testsuites_disabled += testsuites[testsuite_id].Disabled;
    }

    // FIXME: "errors" attribute and <error> tag in <testcase> may be supported if we have means to catch unexpected errors like assertions.
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<testsuites disabled=\"%d\" errors=\"0\" failures=\"%d\" name=\"%s\" tests=\"%d\" time=\"%.3f\">\n",
        testsuites_disabled, testsuites_failures, testsuites_name, testsuites_tests, testsuites_time);

    for (int testsuite_id = ImGuiTestGroup_Tests; testsuite_id < ImGuiTestGroup_COUNT; testsuite_id++)
    {
        // Attributes for <testsuite> tag.
        auto* testsuite = &testsuites[testsuite_id];
        float testsuite_time = testsuites_time;         // FIXME: We do not differentiate between tests and perfs, they are executed in one big batch.
        Str30 testsuite_timestamp = "";
        ImTimestampToISO8601(engine->BatchStartTime, &testsuite_timestamp);
        fprintf(fp, "  <testsuite name=\"%s\" tests=\"%d\" disabled=\"%d\" errors=\"0\" failures=\"%d\" hostname=\"\" id=\"%d\" package=\"\" skipped=\"0\" time=\"%.3f\" timestamp=\"%s\">\n",
            testsuite->Name, testsuite->Tests, testsuite->Disabled, testsuite->Failures, testsuite_id, testsuite_time, testsuite_timestamp.c_str());

        for (int n = 0; n < engine->TestsAll.Size; n++)
        {
            ImGuiTest* test = engine->TestsAll[n];
            if (test->Group != testsuite_id)
                continue;

            ImGuiTestOutput* test_output = &test->Output;
            ImGuiTestLog* test_log = &test_output->Log;

            // Attributes for <testcase> tag.
            const char* testcase_name = test->Name;
            const char* testcase_classname = test->Category;
            const char* testcase_status = ImGuiTestEngine_GetStatusName(test_output->Status);
            const float testcase_time = (float)((double)(test_output->EndTime - test_output->StartTime) / 1000000.0);

            fprintf(fp, "    <testcase name=\"%s\" assertions=\"0\" classname=\"%s\" status=\"%s\" time=\"%.3f\">\n",
                testcase_name, testcase_classname, testcase_status, testcase_time);

            if (test_output->Status == ImGuiTestStatus_Error)
            {
                // Skip last error message because it is generic information that test failed.
                Str128 log_line;
                for (int i = test_log->LineInfo.Size - 2; i >= 0; i--)
                {
                    ImGuiTestLogLineInfo* line_info = &test_log->LineInfo[i];
                    if (line_info->Level > engine->IO.ConfigVerboseLevelOnError)
                        continue;
                    if (line_info->Level == ImGuiTestVerboseLevel_Error)
                    {
                        const char* line_start = test_log->Buffer.c_str() + line_info->LineOffset;
                        const char* line_end = strstr(line_start, "\n");
                        log_line.set(line_start, line_end);
                        ImStrXmlEscape(&log_line);
                        break;
                    }
                }

                // Failing tests save their "on error" log output in text element of <failure> tag.
                fprintf(fp, "      <failure message=\"%s\" type=\"error\">\n", log_line.c_str());
                ImGuiTestEngine_PrintLogLines(fp, test_log, 8, engine->IO.ConfigVerboseLevelOnError);
                fprintf(fp, "      </failure>\n");
            }

            if (test_output->Status == ImGuiTestStatus_Unknown)
            {
                fprintf(fp, "      <skipped message=\"Skipped\" />\n");
            }
            else
            {
                // Succeeding tests save their default log output output as "stdout".
                if (ImGuiTestEngine_HasAnyLogLines(test_log, engine->IO.ConfigVerboseLevel))
                {
                    fprintf(fp, "      <system-out>\n");
                    ImGuiTestEngine_PrintLogLines(fp, test_log, 8, engine->IO.ConfigVerboseLevel);
                    fprintf(fp, "      </system-out>\n");
                }

                // Save error messages as "stderr".
                if (ImGuiTestEngine_HasAnyLogLines(test_log, ImGuiTestVerboseLevel_Error))
                {
                    fprintf(fp, "      <system-err>\n");
                    ImGuiTestEngine_PrintLogLines(fp, test_log, 8, ImGuiTestVerboseLevel_Error);
                    fprintf(fp, "      </system-err>\n");
                }
            }
            fprintf(fp, "    </testcase>\n");
        }

        if (testsuites[testsuite_id].Disabled < testsuites[testsuite_id].Tests) // Any tests executed
        {
            // Log all log messages as "stdout".
            fprintf(fp, "    <system-out>\n");
            for (int n = 0; n < engine->TestsAll.Size; n++)
            {
                ImGuiTest* test = engine->TestsAll[n];
                ImGuiTestOutput* test_output = &test->Output;
                if (test->Group != testsuite_id)
                    continue;
                if (test_output->Status == ImGuiTestStatus_Unknown)
                    continue;
                fprintf(fp, "      [0000] Test: '%s' '%s'..\n", test->Category, test->Name);
                ImGuiTestVerboseLevel level = test_output->Status == ImGuiTestStatus_Error ? engine->IO.ConfigVerboseLevelOnError : engine->IO.ConfigVerboseLevel;
                ImGuiTestEngine_PrintLogLines(fp, &test_output->Log, 6, level);
            }
            ImGuiTestEngine_ExportResultSummary(engine, fp, 6, (ImGuiTestGroup)testsuite_id);
            fprintf(fp, "    </system-out>\n");

            // Log all warning and error messages as "stderr".
            fprintf(fp, "    <system-err>\n");
            for (int n = 0; n < engine->TestsAll.Size; n++)
            {
                ImGuiTest* test = engine->TestsAll[n];
                ImGuiTestOutput* test_output = &test->Output;
                if (test->Group != testsuite_id)
                    continue;
                if (test_output->Status == ImGuiTestStatus_Unknown)
                    continue;
                fprintf(fp, "      [0000] Test: '%s' '%s'..\n", test->Category, test->Name);
                ImGuiTestEngine_PrintLogLines(fp, &test_output->Log, 6, ImGuiTestVerboseLevel_Warning);
            }
            ImGuiTestEngine_ExportResultSummary(engine, fp, 6, (ImGuiTestGroup)testsuite_id);
            fprintf(fp, "    </system-err>\n");
        }
        fprintf(fp, "  </testsuite>\n");
    }
    fprintf(fp, "</testsuites>\n");
    fclose(fp);
    fprintf(stdout, "Saved test results to '%s' successfully.\n", output_file);
}
