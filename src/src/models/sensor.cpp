#include "sensor.h"
#include <math.h>
#include "../json.h"

inline const char *nameof_typecheck(const void *, const char *name) { return name; }
#define nameof(A) nameof_typecheck((void *)&A, #A)

void Sensor::printIndent(Stream &client, int level)
{
    return;
    // client.print("\r\n");
    // for (int i = 0; i < level; i++) client.print("  ");
}

void Sensor::SerializeShort(Stream &client, int indentLevel)
{
    printIndent(client, indentLevel);
    client.printf("{");
    printIndent(client, indentLevel + 1);
    printJsonMember(client, nameof(name), name);
    client.print(",");
    printIndent(client, indentLevel + 1);
    printJsonMember(client, nameof(id), id);
    printIndent(client, indentLevel);
    client.printf("}");
}

void Sensor::Serialize(Stream &client, long currentTimestamp, int indentLevel)
{
    printIndent(client, indentLevel);
    client.printf("{");
    this->SerializeBody(client, currentTimestamp, indentLevel + 1);
    printIndent(client, indentLevel);
    client.printf("}");
}

void Sensor::SerializeBody(Stream &client, long currentTimestamp, int indentLevel)
{
    printIndent(client, indentLevel);
    printJsonMember(client, nameof(name), name);
    client.print(",");
    printIndent(client, indentLevel);
    printJsonMember(client, nameof(id), id);
    client.print(",");
    printIndent(client, indentLevel);
    printJsonMember(client, nameof(model), model);
    client.print(",");
    printIndent(client, indentLevel);
    printJsonMember(client, nameof(serial), serial);
    client.print(",");
    printIndent(client, indentLevel);
    printJsonMember(client, nameof(data), data);
    client.print(",");
    printIndent(client, indentLevel);
    client.printf("\"%s\": %d,", nameof(battery), battery);
    printIndent(client, indentLevel);
    client.printf("\"%s\": %d,", nameof(rssi), (int)((currentTimestamp - timestamp) > OFFLINE_TIMEOUT ? 0 : rssi));
    printIndent(client, indentLevel);
    client.printf("\"%s\": %g,", nameof(humidity), humidity / 10.0);
    printIndent(client, indentLevel);
    client.printf("\"%s\": %g,", nameof(temperature), temperature / 10.0);
    printIndent(client, indentLevel);
    client.printf("\"%s\": %g,", nameof(temperatureAvg), (round(temperatureAvg * 10)) / 10.0);
    printIndent(client, indentLevel);
    client.printf("\"%s\": %lu,", nameof(seriesTimestamp), seriesTimestamp);
    printIndent(client, indentLevel);
    client.printf("\"%s\": %lu", nameof(timestamp), timestamp);
}
