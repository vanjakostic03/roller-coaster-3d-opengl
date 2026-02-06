#version 330 core
out vec4 FragColor;


in vec3 chNormal;
in vec3 chFragPos;
in vec2 chUV;

uniform vec3 uTint;  

uniform vec3 uLightPos1;
uniform vec3 uLightColor1;

uniform vec3 uLightPos2;
uniform vec3 uLightColor2;

uniform vec3 uLightPos3;
uniform vec3 uLightColor3;

uniform vec3 uLightPos4;
uniform vec3 uLightColor4;

uniform vec3 uLightPos5;
uniform vec3 uLightColor5;

uniform vec3 uLightPos6;
uniform vec3 uLightColor6;

uniform vec3 uViewPos;
uniform sampler2D uDiffMap1;

vec3 calcLight(vec3 lightPos, vec3 lightColor, vec3 normal)
{
    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // diffuse
    vec3 lightDir = normalize(lightPos - chFragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - chFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    return ambient + diffuse + specular;
}

void main()
{
    vec3 norm = normalize(chNormal);

    vec3 light1 = calcLight(uLightPos1, uLightColor1, norm);
    vec3 light2 = calcLight(uLightPos2, uLightColor2, norm);
    vec3 light3 = calcLight(uLightPos2, uLightColor2, norm);
    vec3 light4 = calcLight(uLightPos2, uLightColor2, norm);
    vec3 light5 = calcLight(uLightPos2, uLightColor2, norm);
    vec3 light6 = calcLight(uLightPos2, uLightColor2, norm);

    vec3 result = light1 + light2 + light3 + light4;

    vec4 texColor = texture(uDiffMap1, chUV);
    vec3 finalColor = texColor.rgb * result * uTint;
    FragColor = vec4(finalColor, texColor.a);

}
