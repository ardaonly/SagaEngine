// SPDX-License-Identifier: MPL-2.0

using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit.Core;

public static class CanonicalJson
{
    public static byte[] Serialize(object value)
    {
        var node = JsonSerializer.SerializeToNode(value, SerializerOptions)
            ?? throw new InvalidOperationException("Cannot serialize a null canonical document.");
        return Serialize(node);
    }

    public static byte[] Serialize(JsonNode node)
    {
        using var stream = new MemoryStream();
        using (var writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true }))
        {
            WriteNode(writer, node);
        }
        return Encoding.UTF8.GetBytes(Encoding.UTF8.GetString(stream.ToArray()).Replace("\r\n", "\n") + "\n");
    }

    public static string Hash(byte[] bytes) =>
        "sha256:" + Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();

    public static string HashFile(string path) => Hash(File.ReadAllBytes(path));

    public static void Write(string path, object value)
    {
        var parent = Path.GetDirectoryName(Path.GetFullPath(path));
        if (!string.IsNullOrEmpty(parent))
        {
            Directory.CreateDirectory(parent);
        }
        File.WriteAllBytes(path, Serialize(value));
    }

    public static readonly JsonSerializerOptions SerializerOptions = new()
    {
        PropertyNameCaseInsensitive = false,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };

    private static void WriteNode(Utf8JsonWriter writer, JsonNode? node)
    {
        if (node is JsonObject obj)
        {
            writer.WriteStartObject();
            foreach (var property in obj.OrderBy(item => item.Key, StringComparer.Ordinal))
            {
                writer.WritePropertyName(property.Key);
                WriteNode(writer, property.Value);
            }
            writer.WriteEndObject();
            return;
        }
        if (node is JsonArray array)
        {
            writer.WriteStartArray();
            foreach (var item in array)
            {
                WriteNode(writer, item);
            }
            writer.WriteEndArray();
            return;
        }
        if (node is null)
        {
            writer.WriteNullValue();
            return;
        }
        node.WriteTo(writer);
    }
}
