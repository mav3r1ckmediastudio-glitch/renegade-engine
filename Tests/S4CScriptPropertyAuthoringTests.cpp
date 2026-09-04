#include "renegade/bridge/ScriptPropertyAuthoringService.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }

    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }
}

int main()
{
    using namespace renegade::bridge;

    for (const auto type : {
        ScriptPropertyType::Boolean,
        ScriptPropertyType::Integer,
        ScriptPropertyType::Float,
        ScriptPropertyType::String,
        ScriptPropertyType::Colour,
        ScriptPropertyType::Vector2,
        ScriptPropertyType::Vector3,
        ScriptPropertyType::Enum})
    {
        if (!IsS4CEditableScriptPropertyType(type))
            return Fail("an S4C value type was not editable");
    }
    for (const auto type : {
        ScriptPropertyType::EntityReference,
        ScriptPropertyType::AssetReference,
        ScriptPropertyType::Animation,
        ScriptPropertyType::Audio})
    {
        if (IsS4CEditableScriptPropertyType(type))
            return Fail("an S4D reference type leaked into S4C editing");
    }

    std::string error;

    ScriptMetadataPropertyDescriptor speed;
    speed.name = "speed";
    speed.label = "Speed";
    speed.type = ScriptPropertyType::Float;
    speed.hasMinimum = true;
    speed.minimum = 0.0;
    speed.hasMaximum = true;
    speed.maximum = 20.0;
    speed.hasStep = true;
    speed.step = 0.25;

    ScriptPropertyValue speedValue;
    speedValue.name = "wrong_name";
    speedValue.type = ScriptPropertyType::Float;
    speedValue.numberValue = 2.13f;
    if (!NormalizeScriptPropertyForAuthoring(speed, speedValue, error) ||
        speedValue.name != "speed" || !Near(speedValue.numberValue, 2.25f))
    {
        return Fail("float property did not normalize to metadata step/name");
    }
    speedValue.numberValue = -5.0f;
    if (!NormalizeScriptPropertyForAuthoring(speed, speedValue, error) ||
        !Near(speedValue.numberValue, 0.0f))
    {
        return Fail("float property did not clamp to metadata minimum");
    }

    ScriptMetadataPropertyDescriptor count;
    count.name = "count";
    count.type = ScriptPropertyType::Integer;
    count.hasMinimum = true;
    count.minimum = 1.0;
    count.hasMaximum = true;
    count.maximum = 9.0;
    count.hasStep = true;
    count.step = 2.0;

    ScriptPropertyValue countValue;
    countValue.name = "count";
    countValue.type = ScriptPropertyType::Integer;
    countValue.integerValue = 8;
    if (!NormalizeScriptPropertyForAuthoring(count, countValue, error) ||
        countValue.integerValue != 9)
    {
        return Fail("integer property did not honor metadata step");
    }

    ScriptMetadataPropertyDescriptor colour;
    colour.name = "tint";
    colour.type = ScriptPropertyType::Colour;
    ScriptPropertyValue colourValue;
    colourValue.name = "tint";
    colourValue.type = ScriptPropertyType::Colour;
    colourValue.x = -0.2f;
    colourValue.y = 0.5f;
    colourValue.z = 1.4f;
    colourValue.w = 2.0f;
    if (!NormalizeScriptPropertyForAuthoring(colour, colourValue, error) ||
        !Near(colourValue.x, 0.0f) || !Near(colourValue.y, 0.5f) ||
        !Near(colourValue.z, 1.0f) || !Near(colourValue.w, 1.0f))
    {
        return Fail("colour property did not clamp to normalized RGBA");
    }

    ScriptMetadataPropertyDescriptor vector;
    vector.name = "offset";
    vector.type = ScriptPropertyType::Vector3;
    ScriptPropertyValue vectorValue;
    vectorValue.name = "offset";
    vectorValue.type = ScriptPropertyType::Vector3;
    vectorValue.x = std::numeric_limits<float>::infinity();
    if (NormalizeScriptPropertyForAuthoring(vector, vectorValue, error) ||
        error.empty())
    {
        return Fail("non-finite vector property was accepted");
    }

    ScriptMetadataPropertyDescriptor mode;
    mode.name = "mode";
    mode.type = ScriptPropertyType::Enum;
    mode.enumOptions = {
        {"normal", "Normal"},
        {"fast", "Fast"},
        {"slow", "Slow"},
    };
    ScriptPropertyValue modeValue;
    modeValue.name = "mode";
    modeValue.type = ScriptPropertyType::Enum;
    modeValue.textValue = "fast";
    if (!NormalizeScriptPropertyForAuthoring(mode, modeValue, error))
        return Fail("declared enum option was rejected");
    modeValue.textValue = "missing";
    if (NormalizeScriptPropertyForAuthoring(mode, modeValue, error) || error.empty())
        return Fail("undeclared enum option was accepted");

    ScriptMetadataPropertyDescriptor reference;
    reference.name = "target";
    reference.type = ScriptPropertyType::EntityReference;
    ScriptPropertyValue referenceValue;
    referenceValue.name = "target";
    referenceValue.type = ScriptPropertyType::EntityReference;
    if (NormalizeScriptPropertyForAuthoring(reference, referenceValue, error) ||
        error.find("S4D") == std::string::npos)
    {
        return Fail("S4C did not defer reference-backed authoring to S4D");
    }

    ScriptMetadataPropertyDescriptor mismatch;
    mismatch.name = "message";
    mismatch.type = ScriptPropertyType::String;
    ScriptPropertyValue mismatchValue;
    mismatchValue.name = "message";
    mismatchValue.type = ScriptPropertyType::Float;
    if (NormalizeScriptPropertyForAuthoring(mismatch, mismatchValue, error) ||
        error.empty())
    {
        return Fail("metadata/property type mismatch was accepted");
    }

    std::cout << "PASS: S4C typed property authoring normalization\n";
    return 0;
}
