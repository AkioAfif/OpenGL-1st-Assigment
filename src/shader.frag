#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

uniform vec3 material_ambient;
uniform vec3 material_diffuse;
uniform vec3 material_specular;
uniform float material_shininess;

uniform bool useTexture;
uniform sampler2D texture_diffuse;

void main()
{
    vec3 baseDiffuse = material_diffuse;
    if (useTexture) {
        baseDiffuse *= texture(texture_diffuse, TexCoords).rgb;
    }

    vec3 ambient = material_ambient * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseDiffuse * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material_shininess);
    vec3 specular = material_specular * spec * lightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
