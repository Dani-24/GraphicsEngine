#ifdef RELIEF_MAPPING

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 TangentViewPos;
out vec3 TangentLightPos;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    TexCoord = aTexCoord;

    vec4 FragPosWorld = model * vec4(aPos, 1.0);
    FragPos = FragPosWorld.xyz;

    gl_Position = projection * view * FragPosWorld;

    // Calculate tangents & bitangents
    vec3 T = normalize(vec3(model * vec4(aNormal, 0.0)));
    vec3 B = normalize(vec3(model * vec4(aNormal, 0.0)));
    vec3 N = normalize(vec3(model * vec4(aNormal, 0.0)));
    TBN = transpose(mat3(T, B, N));

    // Light & Camera position to tangent space
    TangentViewPos = TBN * viewPos;
    TangentLightPos = TBN * lightPos;
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////////

in vec2 TexCoords;
in vec3 FragPos;
in vec3 TangentViewPos;
in vec3 TangentLightPos;

out vec4 FragColor;

uniform sampler2D diffuseMap;
uniform sampler2D heightMap;

uniform float heightScale;

// Internet
vec2 reliefMapping(vec2 texCoords, vec3 viewDir) {
    float numLayers = 32.0;
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    vec2 deltaTexCoords = viewDir.xy * heightScale / (viewDir.z * numLayers);
    vec2 currentTexCoords = texCoords;

    float currentHeight = texture(heightMap, currentTexCoords).r;
    
    while (currentLayerDepth < currentHeight) {
        currentTexCoords -= deltaTexCoords;
        currentHeight = texture(heightMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterHeight = currentHeight;
    float beforeHeight = texture(heightMap, prevTexCoords).r;
    
    float weight = (currentLayerDepth - afterHeight) / (afterHeight - beforeHeight);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    return finalTexCoords;
}

// Power
vec2 reliefMapping(vec2 texCoords)
{
    int numSteps = 15;

    // Compute the view ray in texture space
    rayTexspace = TBNInverse * worldViewMatrixInverse * rayEyespace;

    // Increment
    vec3 rayIncrementTexspace;
    rayIncrementTexspace.xy = bumpiness * rayTexspace.xy / abs(rayTexspace.z * texSize);
    rayIncrementTexspace.z = 1.0/numSteps;

    // Sampling state
     vec3 samplePositionTexspace = vec3(texCoords, 0.0);
    float sampledDepth = 1.0 - texture(bumpTexture, samplePositionTexspace.xy).r;

    // Linear search
    for (int i = 0; i < numSteps && samplePositionTexspace.z < sampledDepth; ++i)
    {
         samplePositionTexspace += rayIncrementTexspace;    
         sampledDepth = 1.0 - texture(bumpTexture, samplePositionTexspace.xy).r;
    }

    return samplePositionTexspace.xy;
}

void main() {
    vec3 viewDir = normalize(TangentViewPos - FragPos);
    vec2 newTexCoords = reliefMapping(TexCoords, viewDir);
    
    vec3 color = texture(diffuseMap, newTexCoords).rgb;
    FragColor = vec4(color, 1.0);
}

#endif
#endif