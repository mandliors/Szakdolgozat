#version 330

struct Light
{
	vec4 position;
	vec3 ambient, emission;
	float constant, linear, quadratic;
};

struct Material
{
	vec3 ambient, diffuse, specular;
	float shininess, emission;
};

uniform Material material;

uniform int nLights;
uniform Light lights[10];

uniform vec4 color;

in vec4 position;
in vec3 normal;
in vec3 view;

out vec4 fragmentColor;

void main()
{
	vec3 N = normalize(normal);
	vec3 V = normalize(view);
	if (dot(N, V) < 0) N = -N;

	vec3 ambient = material.ambient * color.xyz;
	vec3 diffuse = material.diffuse * color.xyz;
	vec3 emission = material.emission * color.xyz;

	vec3 radiance = emission;

	for (int i = 0; i < nLights; ++i)
	{
		vec3 dir = lights[i].position.xyz * position.w - position.xyz * lights[i].position.w;

		vec3 L = normalize(dir);
		vec3 H = normalize(L + V);
		float cost = max(dot(N, L), 0);
		float cosd = max(dot(N, H), 0);
		float dist = length(dir);
		float attenuation = 1.0f / (lights[i].constant + lights[i].linear * dist + lights[i].quadratic * dist * dist);

		radiance += 
			(ambient * lights[i].ambient
			+ (diffuse * cost + material.specular * pow(cosd, material.shininess))
			* lights[i].emission)
			* attenuation;
	}

	fragmentColor = vec4(radiance, color.w);
}