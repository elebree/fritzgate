#include "sensor.h"
#include <math.h>

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
    client.printf("\"%s\": \"%s\",", nameof(name), name.c_str());
    printIndent(client, indentLevel + 1);
    client.printf("\"%s\": \"%s\"", nameof(id), id.c_str());
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
    client.printf("\"%s\": \"%s\",", nameof(name), name.c_str());
    printIndent(client, indentLevel);
    client.printf("\"%s\": \"%s\",", nameof(id), id.c_str());
    printIndent(client, indentLevel);
    client.printf("\"%s\": \"%s\",", nameof(model), model.c_str());
    printIndent(client, indentLevel);
    client.printf("\"%s\": \"%s\",", nameof(serial), serial.c_str());
    printIndent(client, indentLevel);
    client.printf("\"%s\": \"%s\",", nameof(data), data.c_str());
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
