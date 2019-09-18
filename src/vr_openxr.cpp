/***********************************************************************************************//**
 * \file      vr_openxr.cpp
 *            OpenXR VR module.
 *            This file contains code related to using the OpenXR API for HMDs and controllers.
 *            Both tracking and rendering are implemented.
 ***************************************************************************************************
 * \brief     OpenXR VR module.
 * \copyright 
 * \contributors Multiplexed Reality
 **************************************************************************************************/

#define GLEW_STATIC
#include "glew.h"
#ifdef _WIN32
#include "wglew.h"
#endif

#ifdef _WIN32
#include <Windows.h>
#include <wrl/client.h>  
#include <d3dcompiler.h>
#else
#include <GL/glx.h>
#endif

#include "vr_openxr.h"

#ifdef _WIN32
#include <cstdlib>
#else
#include <string.h>
#include <math.h>
#include <algorithm>
#endif
#include <ctime>

/***********************************************************************************************//**
 * \class                                  VR_OpenXR
 ***************************************************************************************************
 * VR module for using the OpenXR API.
 **************************************************************************************************/

 //																		____________________________
 //_____________________________________________________________________/   GL::vshader_source
 /**
  * Primitive pass-through vertex shader source code.
  */
const char* const VR_OpenXR::GL::vshader_source(STRING(#version 120\n
	attribute vec2 position;
	attribute vec2 uv;
	varying vec2 texcoord;
	void main()
	{
		gl_Position = vec4(position, 0.0, 1.0);
		texcoord = uv;
	}
));

//																		____________________________
//_____________________________________________________________________/    GL::fshader_source
/**
 * Primitive texture look-up shader source code.
 */
const char* const VR_OpenXR::GL::fshader_source(STRING(#version 120\n
	varying vec2 texcoord;
	uniform sampler2D tex;
	uniform vec4 param;
	void main()
	{
		gl_FragColor = pow(texture2D(tex, texcoord), param.zzzz); // GAMMA CORRECTION (1/gamma should be passed as param.z)
	}
));

#ifdef _WIN32
 //																		____________________________
 //_____________________________________________________________________/   D3D::vshader_source
 /**
  * Primitive pass-through vertex shader source code.
  */
const char* const VR_OpenXR::D3D::vshader_source(STRING(
	cbuffer GammaBuffer : register(b0)
	{
		float4 param;
	};
	struct VertexInputType
	{
		float4 position : POSITION;
		float2 tex : TEXCOORD0;
	};
	struct PixelInputType
	{
		float4 position : SV_POSITION;
		float2 tex : TEXCOORD0;
	};

	PixelInputType TextureVertexShader(VertexInputType input)
	{
		PixelInputType output;
		output.position = input.position;
		output.tex = input.tex * param.xy; // normally u,v==1,1; now reduced to aperture.
		return output;
	}
));

//																		____________________________
//_____________________________________________________________________/   D3D::pshader_source
/**
 * Primitive texture look-up shader source code.
 */
const char* const VR_OpenXR::D3D::pshader_source(STRING(
	Texture2D shaderTexture;
	SamplerState SampleType;
	cbuffer GammaBuffer : register(b0)
	{
		float4 param;
	};
	struct PixelInputType
	{
		float4 position : SV_POSITION;
		float2 tex : TEXCOORD0;
	};

	float4 TexturePixelShader(PixelInputType input) : SV_TARGET
	{
		float4 color;
		color = shaderTexture.Sample(SampleType, input.tex);
		color = pow(color, param.z); // GAMMA CORRECTION (1/gamma should be passed as param.z)
		color.a = 1;
		return color;
	}
));
#endif

//                                                                          ________________________
//_________________________________________________________________________/       VR_OpenXR()
/**
 * Class constructor
 */
VR_OpenXR::VR_OpenXR()
	: VR()
	, m_instance(XR_NULL_HANDLE)
	, m_session(XR_NULL_HANDLE)
	, m_appSpace(XR_NULL_HANDLE)
	, m_formFactor(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)
	, m_viewConfigType(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
	, m_environmentBlendMode(XR_ENVIRONMENT_BLEND_MODE_OPAQUE)
	, m_systemId(XR_NULL_SYSTEM_ID)
	, m_colorSwapchainFormat(1)
	, m_sessionState(XR_SESSION_STATE_UNKNOWN)
	, m_inputState({XR_NULL_HANDLE})
	, hmd_type(HMDType_Null)
	, initialized(false)
{
	m_frameState = { XR_TYPE_FRAME_STATE };

	eye_offset_override[Side_Left] = false;
	eye_offset_override[Side_Right] = false;

	memset(&this->gl, 0, sizeof(this->gl));
#ifdef _WIN32
	memset(&this->d3d, 0, sizeof(this->d3d));
#endif

	SET4X4IDENTITY(t_basestation[0]);
	SET4X4IDENTITY(t_basestation[1]);
}

//                                                                          ________________________
//_________________________________________________________________________/     ~VR_Steam()
/**
 * Class destructor
 */
VR_OpenXR::~VR_OpenXR()
{
	if (this->initialized) {
		this->uninit();
	}
}

//                                                                          ________________________
//_________________________________________________________________________/		type()
/**
 * Get which API was used in this implementation.
 */
VR::Type VR_OpenXR::type()
{
	return VR::Type_OpenXR;
};

//                                                                          ________________________
//_________________________________________________________________________/		hmdType()
/**
 * Get which HMD was used in this implementation.
 */
VR::HMDType VR_OpenXR::hmdType()
{
	return this->hmd_type;
};

//                                                                          ________________________
//_________________________________________________________________________/     GL::create()
/**
 * Create required OpenGL objects.
 */
bool VR_OpenXR::GL::create(uint width, uint height)
{
	bool success = true; // assume success

	// Create texture targets / frame buffers
	for (int i = 0; i < Sides; ++i) {
		glGenFramebuffers(1, &this->framebuffer[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer[i]);

		glGenTextures(1, &this->texture[i]);
		glBindTexture(GL_TEXTURE_2D, this->texture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->texture[i], 0);

		// check FBO status
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			success = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Create vertex buffer
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	// Flip for d3d coordinates
	static const GLfloat vertex_data[] = {
		-1.0f,  1.0f,
		1.0f,  1.0f,
		-1.0f, -1.0f,
		1.0f, -1.0f,
	};
#else
	static const GLfloat vertex_data[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f,
	};
#endif
#else
	static const GLfloat vertex_data[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f,
	};
#endif
	glGenBuffers(1, &this->verts);
	glBindBuffer(GL_ARRAY_BUFFER, this->verts);
	glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float), vertex_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Create uv buffer
	static const GLfloat uv_data[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
	};
	glGenBuffers(1, &this->uvs);
	glBindBuffer(GL_ARRAY_BUFFER, this->uvs);
	glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(float), uv_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Create shaders required for texture blitting
	this->program = glCreateProgram();
	this->vshader = glCreateShader(GL_VERTEX_SHADER);
	this->fshader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(this->vshader, 1, &GL::vshader_source, 0);
	glShaderSource(this->fshader, 1, &GL::fshader_source, 0);

	GLint ret;

	glCompileShader(this->vshader);
	glGetShaderiv(this->vshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetShaderInfoLog(this->vshader, 256, &len, error);
		error[255] = 0;
		success = false;
	}
	glAttachShader(this->program, this->vshader);

	glCompileShader(this->fshader);
	glGetShaderiv(this->fshader, GL_COMPILE_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetShaderInfoLog(this->fshader, 256, &len, error);
		error[255] = 0;
		success = false;
	}
	glAttachShader(this->program, this->fshader);

	glLinkProgram(this->program);
	glGetProgramiv(this->program, GL_LINK_STATUS, &ret);
	if (!ret) {
		GLchar error[256];
		GLsizei len;
		glGetProgramInfoLog(this->program, 256, &len, error);
		error[255] = 0;
		success = false;
	}

	this->position_location = glGetAttribLocation(this->program, "position");
	this->uv_location = glGetAttribLocation(this->program, "uv");
	this->sampler_location = glGetUniformLocation(this->program, "tex");
	glUniform1i(this->sampler_location, 0);
	this->param_location = glGetUniformLocation(this->program, "param");

	// Create vertex array
	glGenVertexArrays(1, &this->vertex_array);
	glBindVertexArray(this->vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, this->verts);
	glVertexAttribPointer(this->position_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, this->uvs);
	glVertexAttribPointer(this->uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return success;
}

//                                                                          ________________________
//_________________________________________________________________________/   GL::release()
/**
 * Release OpenGL objects.
 */
void VR_OpenXR::GL::release()
{
	for (int i = 0; i < Sides; ++i) {
		if (this->framebuffer[i]) {
			glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glDeleteFramebuffers(1, &this->framebuffer[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			this->framebuffer[i] = 0;
		}
		if (this->texture[i]) {
			glDeleteTextures(1, &this->texture[i]);
			this->texture[i] = 0;
		}
	}

	if (this->program) {
		glDeleteProgram(this->program);
		this->program = 0;
	}
	if (this->vshader) {
		glDeleteShader(this->vshader);
		this->vshader = 0;
	}
	if (this->fshader) {
		glDeleteShader(this->fshader);
		this->fshader = 0;
	}
}

#ifdef _WIN32
//                                                                          ________________________
//_________________________________________________________________________/   D3D::create()
/**
 * Create required Direct3D objects.
 */
bool VR_OpenXR::D3D::create(uint width, uint height)
{
	bool success = true; // assume success

	HRESULT hr;
	ID3D11Device* d3d_device = (ID3D11Device*)device;
	ID3D11DeviceContext* d3d_context = (ID3D11DeviceContext*)context;

	// Setup the render target texture
	for (int i = 1; i >= 0; i--) {
		D3D11_TEXTURE2D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(textureDesc));
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;
		hr = d3d_device->CreateTexture2D(&textureDesc, NULL, &this->texture[i]);
		if (FAILED(hr)) {
			success = false;
		}

		// Setup the render target view.
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format = textureDesc.Format;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;
		hr = d3d_device->CreateRenderTargetView(this->texture[i], &renderTargetViewDesc, &this->view[i]);
		if (FAILED(hr)) {
			success = false;
		}
	}

	ID3DBlob* error_messages = 0;
	ID3DBlob* compiled_vshader_buffer = 0;
	hr = D3DCompile(D3D::vshader_source,
		strlen(D3D::vshader_source),
		"BlenderXR_Texture_VShader",
		NULL, // macros, D3D_SHADER_MACRO macros[1] = {0};
		NULL,
		"TextureVertexShader",
		"vs_5_0",               // D3D11 Vertex shader compiler target
		D3D10_SHADER_ENABLE_STRICTNESS, // D3DCOMPILE_DEBUG,
		0,
		&compiled_vshader_buffer,
		&error_messages);
	if (FAILED(hr)) {
		success = false;
	}

	hr = d3d_device->CreateVertexShader(compiled_vshader_buffer->GetBufferPointer(),
		compiled_vshader_buffer->GetBufferSize(),
		NULL,
		&vertex_shader);
	if (FAILED(hr)) {
		success = false;
	}

	ID3DBlob* compiled_pshader_buffer = 0;
	hr = D3DCompile(D3D::pshader_source,
		strlen(D3D::pshader_source),
		"BlenderXR_Texture_PShader",
		NULL, // macros, D3D_SHADER_MACRO macros[1] = {0};
		NULL,
		"TexturePixelShader",
		"ps_5_0",                   // D3D11 Pixel shader
		D3D10_SHADER_ENABLE_STRICTNESS, // D3DCOMPILE_DEBUG,
		0,
		&compiled_pshader_buffer,
		&error_messages);
	if (FAILED(hr)) {
		success = false;
	}

	hr = d3d_device->CreatePixelShader(compiled_pshader_buffer->GetBufferPointer(),
		compiled_pshader_buffer->GetBufferSize(),
		NULL,
		&this->pixel_shader);
	if (FAILED(hr)) {
		success = false;
	}

	d3d_context->VSSetShader(this->vertex_shader, 0, 0);
	d3d_context->PSSetShader(this->pixel_shader, 0, 0);

	// Create the vertex input layout description (to match the Vertex type)
	D3D11_INPUT_ELEMENT_DESC polygon_layout[2];
	polygon_layout[0].SemanticName = "POSITION";
	polygon_layout[0].SemanticIndex = 0;
	polygon_layout[0].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	polygon_layout[0].InputSlot = 0;
	polygon_layout[0].AlignedByteOffset = 0;
	polygon_layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygon_layout[0].InstanceDataStepRate = 0;
	polygon_layout[1].SemanticName = "TEXCOORD";
	polygon_layout[1].SemanticIndex = 0;
	polygon_layout[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	polygon_layout[1].InputSlot = 0;
	polygon_layout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygon_layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygon_layout[1].InstanceDataStepRate = 0;

	// Create the vertex input layout.
	hr = d3d_device->CreateInputLayout(polygon_layout, 2,
		compiled_vshader_buffer->GetBufferPointer(),
		compiled_vshader_buffer->GetBufferSize(),
		&this->input_layout);
	if (FAILED(hr)) {
		success = false;
	}

	compiled_vshader_buffer->Release();
	compiled_pshader_buffer->Release();

	// Create the texture sampler state.
	D3D11_SAMPLER_DESC sampler_desc;
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MipLODBias = 0.0f;
	sampler_desc.MaxAnisotropy = 1;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 0;
	sampler_desc.MinLOD = 0;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = d3d_device->CreateSamplerState(&sampler_desc, &this->sampler_state);
	if (FAILED(hr)) {
		success = false;
	}

	D3D11_BUFFER_DESC vertex_buffer_desc;
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.ByteWidth = sizeof(D3D::Vertex) * 4;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertex_buffer_desc.CPUAccessFlags = 0;
	vertex_buffer_desc.MiscFlags = 0;
	vertex_buffer_desc.StructureByteStride = 0;

	D3D::Vertex vertices[4];
	vertices[0].pos.x = -1;    vertices[0].pos.y = -1;    vertices[0].pos.z = 0;    vertices[0].tex.x = 0;    vertices[0].tex.y = 1;
	vertices[1].pos.x = -1;    vertices[1].pos.y = 1;    vertices[1].pos.z = 0;    vertices[1].tex.x = 0;    vertices[1].tex.y = 0;
	vertices[2].pos.x = 1;    vertices[2].pos.y = 1;    vertices[2].pos.z = 0;    vertices[2].tex.x = 1;    vertices[2].tex.y = 0;
	vertices[3].pos.x = 1;    vertices[3].pos.y = -1;    vertices[3].pos.z = 0;    vertices[3].tex.x = 1;    vertices[3].tex.y = 1;

	D3D11_SUBRESOURCE_DATA vertex_buffer_data;
	vertex_buffer_data.pSysMem = vertices;
	vertex_buffer_data.SysMemPitch = 0;
	vertex_buffer_data.SysMemSlicePitch = 0;

	hr = d3d_device->CreateBuffer(&vertex_buffer_desc, &vertex_buffer_data, &this->vertex_buffer);
	if (FAILED(hr)) {
		success = false;
	}

	D3D11_BUFFER_DESC index_buffer_desc;
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.ByteWidth = sizeof(int32_t) * 4;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	index_buffer_desc.CPUAccessFlags = 0;
	index_buffer_desc.MiscFlags = 0;
	index_buffer_desc.StructureByteStride = 0;

	int32_t indices[4];
	indices[0] = 1;
	indices[1] = 2;
	indices[2] = 0;
	indices[3] = 3;

	D3D11_SUBRESOURCE_DATA index_buffer_data;
	index_buffer_data.pSysMem = indices;
	index_buffer_data.SysMemPitch = 0;
	index_buffer_data.SysMemSlicePitch = 0;

	hr = d3d_device->CreateBuffer(&index_buffer_desc, &index_buffer_data, &this->index_buffer);
	if (FAILED(hr)) {
		success = false;
	}

	D3D11_BUFFER_DESC param_buffer_desc;
	param_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	param_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	param_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	param_buffer_desc.MiscFlags = 0;
	param_buffer_desc.StructureByteStride = 0;
	param_buffer_desc.ByteWidth = sizeof(DirectX::XMFLOAT4);
	hr = d3d_device->CreateBuffer(&param_buffer_desc, NULL, &this->param_buffer);
	if (FAILED(hr)) {
		success = false;
	}

	// Setup the raster description which will determine how and what polygons will be drawn.
	D3D11_RASTERIZER_DESC raster_desc;
	raster_desc.AntialiasedLineEnable = false;
	raster_desc.CullMode = D3D11_CULL_NONE; //  D3D11_CULL_BACK;
	raster_desc.DepthBias = 0;
	raster_desc.DepthBiasClamp = 0.0f;
	raster_desc.DepthClipEnable = false; //  true;
	raster_desc.FillMode = D3D11_FILL_SOLID;
	raster_desc.FrontCounterClockwise = false;
	raster_desc.MultisampleEnable = false;
	raster_desc.ScissorEnable = false;
	raster_desc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	hr = d3d_device->CreateRasterizerState(&raster_desc, &this->rasterizer_state);
	if (FAILED(hr)) {
		success = false;
	}

	return success;
}

//                                                                          ________________________
//_________________________________________________________________________/   D3D::release()
/**
 * Release Direct3D objects.
 */
void VR_OpenXR::D3D::release()
{
	for (int i = 1; i >= 0; i--) {
		if (this->view[i]) {
			this->view[i]->Release();
			this->view[i] = NULL;
		}
		if (this->texture[i]) {
			this->texture[i]->Release();
			this->texture[i] = NULL;
		}
	}

	if (this->sampler_state) {
		this->sampler_state->Release();
		this->sampler_state = NULL;
	}
	if (this->rasterizer_state) {
		this->rasterizer_state->Release();
		this->rasterizer_state = NULL;
	}
	if (this->vertex_shader) {
		this->vertex_shader->Release();
		this->vertex_shader = NULL;
	}
	if (this->pixel_shader) {
		this->pixel_shader->Release();
		this->pixel_shader = NULL;
	}
	if (this->input_layout) {
		this->input_layout->Release();
		this->input_layout = NULL;
	}
	if (this->vertex_buffer) {
		this->vertex_buffer->Release();
		this->vertex_buffer = NULL;
	}
	if (this->index_buffer) {
		this->index_buffer->Release();
		this->index_buffer = NULL;
	}
	if (this->param_buffer) {
		this->param_buffer->Release();
		this->param_buffer = NULL;
	}

	if (this->context) {
		this->context->Release();
		this->context = NULL;
	}
	if (this->device) {
		this->device->Release();
		this->device = NULL;
	}
}
#endif

//                                                                          ________________________
//_________________________________________________________________________/      acquireHMD()
/**
 * Initialize basic OVR operation and acquire the HMD object.
 * \return  Zero on success, and error code on failue.
 */
int VR_OpenXR::acquireHMD()
{
	if (m_instance || m_session) {
		this->releaseHMD();
	}
	
	// Create XR instance
	uint32_t layer_count = 0;
	if (XR_FAILED(xrEnumerateApiLayerProperties(0, &layer_count, nullptr))) {
		return VR::Error_InternalFailure;
	}

	std::vector<XrExtensionProperties> extensions;
	if (layer_count > 0) {
		// Get layers
		std::vector<XrApiLayerProperties> layers(layer_count);
		for (XrApiLayerProperties& layer : layers) {
			layer.type = XR_TYPE_API_LAYER_PROPERTIES;
		}
		xrEnumerateApiLayerProperties(layer_count, &layer_count, layers.data());

		// Get extensions
		for (XrApiLayerProperties& layer : layers) {
			uint32_t extension_count = 0;
			const char *layer_name = layer.layerName;
			if (XR_FAILED(xrEnumerateInstanceExtensionProperties(layer_name, 0, &extension_count, nullptr))) {
				return VR::Error_InternalFailure;
			}

			if (extension_count == 0) {
				continue;
			}

			for (uint32_t i = 0; i < extension_count; ++i) {
				XrExtensionProperties ext{};
				ext.type = XR_TYPE_EXTENSION_PROPERTIES;
				extensions.push_back(ext);
			}

			xrEnumerateInstanceExtensionProperties(layer_name, extension_count, &extension_count, extensions.data());
		}
	}
	else {
		uint32_t extension_count = 0;
		if (XR_FAILED(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extension_count, nullptr))) {
			return VR::Error_InternalFailure;
		}

		for (uint32_t i = 0; i < extension_count; ++i) {
			XrExtensionProperties ext{};
			ext.type = XR_TYPE_EXTENSION_PROPERTIES;
			extensions.push_back(ext);
		}

		xrEnumerateInstanceExtensionProperties(nullptr, extension_count, &extension_count, extensions.data());
	}

	XrInstanceCreateInfo create_info{};
	create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
	std::string("BlenderXR").copy(create_info.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE);
	create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

	// check if OpenGL / D3D11 extension is available
	std::vector<const char*> enabled_extensions;
	bool graphics_ext_found = false;
	for (XrExtensionProperties& extension : extensions) {
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
		if (strcmp(extension.extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0) {
			graphics_ext_found = true;
			enabled_extensions.push_back(extension.extensionName);
			break;
		}
#else
		if (strcmp(extension.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) {
			graphics_ext_found = true;
			enabled_extensions.push_back(extension.extensionName);
			break;
		}
#endif
#else
		if (strcmp(extension.extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) {
			enabled_extensions.push_back(extension.extensionName);
			graphics_ext_found = true;
			break;
		}
#endif
		//enabled_extensions.push_back(extension.extensionName);
	}
	if (!graphics_ext_found) {
		return VR::Error_NotAvailable;
	}

	create_info.enabledExtensionCount = enabled_extensions.size();
	create_info.enabledExtensionNames = enabled_extensions.data();
	if (XR_FAILED(xrCreateInstance(&create_info, &m_instance))) {
		return VR::Error_InternalFailure;
	}

	// Initialize XR based on system
	m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	m_viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

	XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = m_formFactor;
	if (XR_FAILED(xrGetSystem(m_instance, &systemInfo, &m_systemId))) {
		return VR::Error_InternalFailure;
	}

	// Create XR session
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	// Create d3d device and context
	XrGraphicsRequirementsD3D11KHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	if (XR_FAILED(xrGetD3D11GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements))) {
		return VR::Error_InternalFailure;
	}
	
	// Create the DXGI factory.
	Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return VR::Error_InternalFailure;
	}

	// Get adapter.
	LUID adapterId = graphicsRequirements.adapterLuid;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter = NULL;
	for (UINT adapterIndex = 0;; ++adapterIndex) {
		// EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to enumerate.
		Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgiAdapter;
		hr = dxgiFactory->EnumAdapters1(adapterIndex, dxgiAdapter.ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			return VR::Error_InternalFailure;
		}

		DXGI_ADAPTER_DESC1 adapterDesc;
		hr = dxgiAdapter->GetDesc1(&adapterDesc);
		if (FAILED(hr)) {
			return VR::Error_InternalFailure;
		}
		if (memcmp(&adapterDesc.AdapterLuid, &adapterId, sizeof(adapterId)) == 0) {
			adapter = dxgiAdapter;
			break;
		}
	}
	if (!adapter) {
		return VR::Error_NotAvailable;
	}

	// Create a list of feature levels which are both supported by the OpenXR runtime and this application.
	std::vector<D3D_FEATURE_LEVEL> featureLevels = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
													 D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
	featureLevels.erase(std::remove_if(featureLevels.begin(), featureLevels.end(),
		[&](D3D_FEATURE_LEVEL fl) { return fl < graphicsRequirements.minFeatureLevel; }),
		featureLevels.end());
	if (featureLevels.size() < 1) {
		return VR::Error_NotAvailable;
	}

	// Create the Direct3D 11 API device object and a corresponding context.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	const D3D_DRIVER_TYPE driverType = adapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN;
	hr = D3D11CreateDevice(adapter.Get(), driverType, 0, creationFlags, featureLevels.data(), (UINT)featureLevels.size(),
		D3D11_SDK_VERSION, (ID3D11Device**)&(d3d.device), nullptr, (ID3D11DeviceContext**)&(d3d.context));
	if (FAILED(hr)) {
		return VR::Error_NotAvailable;
	}

	// Create XR graphics binding.
	XrGraphicsBindingD3D11KHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	graphicsBinding.device = d3d.device;
#else
	XrGraphicsBindingOpenGLWin32KHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
	graphicsBinding.hDC = gl.device;
	graphicsBinding.hGLRC = gl.context;
#endif
#else
	XrGraphicsBindingOpenGLXlibKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR };
	graphicsBinding.xDisplay = gl.display;
	graphicsBinding.glxDrawable = gl.drawable;
	graphicsBinding.glxContext = gl.context;
#endif

	XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
	sessionCreateInfo.systemId = m_systemId;
	sessionCreateInfo.next = &graphicsBinding;
	if (XR_FAILED(xrCreateSession(m_instance, &sessionCreateInfo, &m_session))) {
		return VR::Error_InternalFailure;
	}

	// Figure out which HMD it is
	XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
	if (XR_FAILED(xrGetSystemProperties(m_instance, m_systemId, &systemProperties))) {
		return VR::Error_InternalFailure;
	}

	/* TODO_XR: Test for each supported HMD */
	const char *systemName = systemProperties.systemName; 
	const int strSize = sizeof(systemProperties.systemName);
	//const int& vendorID = systemProperties.vendorId;

	// For Oculus Rift / Rift S:
	// systemName == ?
	// vendorID == ?
	if (strncmp(systemName, "Oculus", strSize) == 0) {
		this->hmd_type = VR::HMDType_Oculus;
		return VR::Error_None;
	}
	// For HTC Vive / Vive Pro:
	// systemName == ?
	// vendorID == ?
	else if (strncmp(systemName, "HTC Vive", strSize) == 0) {
		this->hmd_type = VR::HMDType_Vive;
		return VR::Error_None;
	}
	// For WindowsMR:
	// systemName == Windows Mixed Reality
	// vendorID == 1118 (Lenovo Explorer)
	else if (strncmp(systemName, "Windows Mixed Reality", strSize) == 0) {
		this->hmd_type = VR::HMDType_WindowsMR;
		return VR::Error_None;
	}
	// For Fove0:
	// systemName == ?
	// vendorID == ?
	else if (strncmp(systemName, "Fove", strSize) == 0) {
		this->hmd_type = VR::HMDType_Fove;
		return VR::Error_None;
	}
	// For Pimax:
	// systemName == ?
	// vendorID == ?
	else if (strncmp(systemName, "Pimax", strSize) == 0) {
		this->hmd_type = VR::HMDType_Pimax;
		return VR::Error_None;
	}
	// For Valve Index:
	// systemName == ?
	// vendorID == ?
	else if (strncmp(systemName, "Valve Index", strSize) == 0) {
		this->hmd_type = VR::HMDType_Index;
		return VR::Error_None;
	}

	// If we arrive here, we could not find any supported HMD
	return VR::Error_InternalFailure;
}

//                                                                          ________________________
//_________________________________________________________________________/    releaseHMD()
/**
 * Delete the HMD object and uninitialize basic OVR operation.
 * \return  Zero on success, and error code on failue.
 */
int VR_OpenXR::releaseHMD()
{
	// Shutdown VR
	if (m_session != XR_NULL_HANDLE) {
		xrDestroySession(m_session);
		m_session = XR_NULL_HANDLE;
	}
	if (m_instance != XR_NULL_HANDLE) {
		xrDestroyInstance(m_instance);
		m_instance = XR_NULL_HANDLE;
	}

	return VR::Error_None;
}

#ifdef _WIN32
//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   device          The graphics device used by Blender (HDC)
 * \param   context         The rendering context used by Blennder (HGLRC)
 * \return  Zero on success, an error code on failure.
 */
int VR_OpenXR::init(void* device, void* context)
#else
//                                                                          ________________________
//_________________________________________________________________________/        init()
/**
 * Initialize the VR device.
 * \param   display     The connection to the X server (Display*).
 * \param   drawable    Pointer to the GLX drawable (GLXDrawable*). Either an X window ID or a GLX pixmap ID.
 * \param   context     Pointer to the GLX rendering context (GLXContext*) attached to drawable.
 * \return  Zero on success, an error code on failure.
 */
int VR_OpenXR::init(void* display, void* drawable, void* context)
#endif
{
	if (this->initialized) {
		this->uninit();
	}

	// Get the Blender viewport window / context IDs.
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	// Need GL to DX capabilities for Win32
	if (!(WGL_NV_DX_interop && WGL_NV_DX_interop2)) {
		return VR::Error_NotAvailable;
	}
#else
	this->gl.device = (HDC)device;
	this->gl.context = (HGLRC)context; // save old context so that we can share
#endif
#else
	this->gl.display = (Display*)display;
	this->gl.drawable = *(GLXDrawable*)drawable;
	this->gl.context = *(GLXContext*)context;
#endif

	if (!m_instance || !m_session) {
		int error = this->acquireHMD();
		if (error) {
			releaseHMD();
			return VR::Error_InternalFailure;
		}
	}

	// Initialize swapchain / compositor
	// Query and cache view configuration views.
	uint32_t viewCount;
	if (XR_FAILED(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewConfigType, 0, &viewCount, nullptr))) {
		releaseHMD();
		return VR::Error_InternalFailure;
	}
	m_configViews.resize(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	if (XR_FAILED(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewConfigType, viewCount, &viewCount, m_configViews.data()))) {
		releaseHMD();
		return VR::Error_InternalFailure;
	}

	if (viewCount < 0) {
		releaseHMD();
		return VR::Error_InternalFailure;
	}

	// Create and cache view buffer for xrLocateViews later.
	m_views.resize(viewCount, { XR_TYPE_VIEW });

	// Initialize glew
	glewExperimental = GL_TRUE; // This is required for the glGenFramebuffers call
	if (glewInit() != GLEW_OK) {
		return VR::Error_InternalFailure;
	}

	// Create the render buffers and textures
	XrViewConfigurationView& configView = m_configViews[0];
	texture_width = configView.recommendedImageRectWidth;
	texture_height = configView.recommendedImageRectHeight;
	if (!this->gl.create(this->texture_width, this->texture_height)) {
		return VR::Error_InternalFailure;
	}

#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	if (!this->d3d.create(this->texture_width, this->texture_height)) {
		return VR::Error_InternalFailure;
	}

	// Create shared device and textures
	shared_device = wglDXOpenDeviceNV(d3d.device);
	if (!shared_device) {
		return VR::Error_InternalFailure;
	}
	for (int i = 0; i < Sides; ++i) {
		shared_texture[i] = wglDXRegisterObjectNV(
			shared_device,
			d3d.texture[i],
			gl.texture[i],
			GL_TEXTURE_2D,
			WGL_ACCESS_READ_WRITE_NV);
		if (!shared_texture[i]) {
			return VR::Error_InternalFailure;
		}
	}
#endif
#endif

	// Select a swapchain format.
	uint32_t swapchainFormatCount;
	if (XR_FAILED(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr))) {
		return VR::Error_InternalFailure;
	}
	std::vector<int64_t> swapchainFormats(swapchainFormatCount);
	if (XR_FAILED(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
		swapchainFormats.data())) || swapchainFormatCount != swapchainFormats.size()) {
		return VR::Error_InternalFailure;
	}
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	constexpr DXGI_FORMAT SupportedColorSwapchainFormats[] = {
		   DXGI_FORMAT_R8G8B8A8_UNORM,
		   //DXGI_FORMAT_B8G8R8A8_UNORM,
		   //DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		   //DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
	};
#else
	constexpr int64_t SupportedColorSwapchainFormats[] = {
			GL_RGBA8,
			//GL_RGBA8_SNORM,
	};
#endif
#else
	constexpr int64_t SupportedColorSwapchainFormats[] = {
			GL_RGBA8,
			//GL_RGBA8_SNORM,
	};
#endif
	auto swapchainFormatIt =
		std::find_first_of(std::begin(SupportedColorSwapchainFormats), std::end(SupportedColorSwapchainFormats),
			swapchainFormats.begin(), swapchainFormats.end());
	if (swapchainFormatIt == std::end(SupportedColorSwapchainFormats)) {
		return VR::Error_InternalFailure;
	}
	m_colorSwapchainFormat = *swapchainFormatIt;

	// Create a swapchain for each view.
	for (uint32_t i = 0; i < viewCount; ++i) {
		const XrViewConfigurationView& vp = m_configViews[i];

		// Create the swapchain.
		XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
		swapchainCreateInfo.arraySize = 1;
		swapchainCreateInfo.format = m_colorSwapchainFormat;
		swapchainCreateInfo.width = vp.recommendedImageRectWidth;
		swapchainCreateInfo.height = vp.recommendedImageRectHeight;
		swapchainCreateInfo.mipCount = 1;
		swapchainCreateInfo.faceCount = 1;
		swapchainCreateInfo.sampleCount = vp.recommendedSwapchainSampleCount;
		swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		Swapchain swapchain;
		swapchain.width = swapchainCreateInfo.width;
		swapchain.height = swapchainCreateInfo.height;
		if (XR_FAILED(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle))) {
			releaseHMD();
			return VR::Error_InternalFailure;
		}

		m_swapchains.push_back(swapchain);

		uint32_t imageCount;
		if (XR_FAILED(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr))) {
			releaseHMD();
			return VR::Error_InternalFailure;
		}
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
		std::vector<XrSwapchainImageD3D11KHR> swapchainImageBuffer(imageCount);
		std::vector<XrSwapchainImageBaseHeader*> swapchainImageBase;
		for (XrSwapchainImageD3D11KHR& image : swapchainImageBuffer) {
			image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
			swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
		}
#else
		std::vector<XrSwapchainImageOpenGLKHR> swapchainImageBuffer(imageCount);
		std::vector<XrSwapchainImageBaseHeader*> swapchainImageBase;
		for (XrSwapchainImageOpenGLKHR& image : swapchainImageBuffer) {
			image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
			swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
		}
#endif
#else
		std::vector<XrSwapchainImageOpenGLKHR> swapchainImageBuffer(imageCount);
		std::vector<XrSwapchainImageBaseHeader*> swapchainImageBase;
		for (XrSwapchainImageOpenGLKHR& image : swapchainImageBuffer) {
			image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
			swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
		}
#endif
		// Keep the buffer alive by moving it into the list of buffers.
		m_swapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

		if (XR_FAILED(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImageBase[0]))) {
			releaseHMD();
			return VR::Error_InternalFailure;
		}

		m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImageBase)));
	}

	// Create action sets
	XrActionSetCreateInfo actionSetInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy(actionSetInfo.actionSetName, "gameplay");
	strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
	actionSetInfo.priority = 0;
	if (XR_FAILED(xrCreateActionSet(m_instance, &actionSetInfo, &m_inputState.actionSet))) {
		return VR::Error_InternalFailure;
	}

	// Create subactions for left and right hands.
	if (XR_FAILED(xrStringToPath(m_instance, "/user/head", &m_inputState.headSubactionPath))) {
		return VR::Error_InternalFailure;
	}
	if (XR_FAILED(xrStringToPath(m_instance, "/user/hand/left", &m_inputState.handSubactionPath[Side_Left]))) {
		return VR::Error_InternalFailure;
	}
	if (XR_FAILED(xrStringToPath(m_instance, "/user/hand/right", &m_inputState.handSubactionPath[Side_Right]))) {
		return VR::Error_InternalFailure;
	}

	// Create actions.
	// Create an input action getting the head pose.
	XrActionCreateInfo actionInfo{ XR_TYPE_ACTION_CREATE_INFO };
	actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy(actionInfo.actionName, "head_pose");
	strcpy(actionInfo.localizedActionName, "Head Pose");
	actionInfo.countSubactionPaths = 1;
	actionInfo.subactionPaths = &m_inputState.headSubactionPath;
	if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.headPoseAction))) {
		return VR::Error_InternalFailure;
	}

	// Create an input action getting the left and right hand poses.
	actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy(actionInfo.actionName, "hand_pose");
	strcpy(actionInfo.localizedActionName, "Hand Pose");
	actionInfo.countSubactionPaths = uint32_t(m_inputState.handSubactionPath.size());
	actionInfo.subactionPaths = m_inputState.handSubactionPath.data();
	if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.handPoseAction))) {
		return VR::Error_InternalFailure;
	}

	// Create input actions for button states.
	switch (this->hmd_type) {
	case HMDType_Oculus: {
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "trigger_touch");
		strcpy(actionInfo.localizedActionName, "Trigger Touch");
		actionInfo.countSubactionPaths = uint32_t(m_inputState.handSubactionPath.size());
		actionInfo.subactionPaths = m_inputState.handSubactionPath.data();
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trigger_value");
		strcpy(actionInfo.localizedActionName, "Trigger Value");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerValueAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "grip_value");
		strcpy(actionInfo.localizedActionName, "Grip Value");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.gripValueAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_x");
		strcpy(actionInfo.localizedActionName, "Thumbstick X");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickXAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_y");
		strcpy(actionInfo.localizedActionName, "Thumbstick Y");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickYAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_touch");
		strcpy(actionInfo.localizedActionName, "Thumbstick Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_click");
		strcpy(actionInfo.localizedActionName, "Thumbstick Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbrest_touch");
		strcpy(actionInfo.localizedActionName, "Thumbrest Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbrestTouchAction))) {
			return VR::Error_InternalFailure;
		}

		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "X_touch");
		strcpy(actionInfo.localizedActionName, "X Touch");
		actionInfo.countSubactionPaths = 1;
		actionInfo.subactionPaths = &m_inputState.handSubactionPath[Side_Left];
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.XTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "X_click");
		strcpy(actionInfo.localizedActionName, "X Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.XClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "Y_touch");
		strcpy(actionInfo.localizedActionName, "X Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.YTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "Y_click");
		strcpy(actionInfo.localizedActionName, "Y Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.YClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "menu_click");
		strcpy(actionInfo.localizedActionName, "Menu Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.menuClickAction))) {
			return VR::Error_InternalFailure;
		}

		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "A_touch");
		strcpy(actionInfo.localizedActionName, "A Touch");
		actionInfo.countSubactionPaths = 1;
		actionInfo.subactionPaths = &m_inputState.handSubactionPath[Side_Right];
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.ATouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "A_click");
		strcpy(actionInfo.localizedActionName, "A Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.AClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "B_touch");
		strcpy(actionInfo.localizedActionName, "B Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.BTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "B_click");
		strcpy(actionInfo.localizedActionName, "B Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.BClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "system_click");
		strcpy(actionInfo.localizedActionName, "System Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.systemClickAction))) {
			return VR::Error_InternalFailure;
		}

		XrPath headPosePath;
		std::array<XrPath, Sides> handPosePath;
		std::array<XrPath, Sides> triggerTouchPath;
		std::array<XrPath, Sides> triggerValuePath;
		std::array<XrPath, Sides> gripValuePath;
		std::array<XrPath, Sides> thumbstickXPath;
		std::array<XrPath, Sides> thumbstickYPath;
		std::array<XrPath, Sides> thumbstickTouchPath;
		std::array<XrPath, Sides> thumbstickClickPath;
		std::array<XrPath, Sides> thumbrestTouchPath;
		XrPath XTouchPath;
		XrPath XClickPath;
		XrPath YTouchPath;
		XrPath YClickPath;
		XrPath ATouchPath;
		XrPath AClickPath;
		XrPath BTouchPath;
		XrPath BClickPath;
		XrPath menuClickPath;
		XrPath systemClickPath;

		xrStringToPath(m_instance, "/user/head/input/pose" /*"/user/head/pose"*/, &headPosePath);
		xrStringToPath(m_instance, "/user/hand/left/input/grip/pose", &handPosePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &handPosePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/touch", &triggerTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/touch", &triggerTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value", &gripValuePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value", &gripValuePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/x", &thumbstickXPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/x", &thumbstickXPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/y", &thumbstickYPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/y", &thumbstickYPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/touch", &thumbstickTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/touch", &thumbstickTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/click", &thumbstickClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbrest/touch", &thumbrestTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbrest/touch", &thumbrestTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/x/touch", &XTouchPath);
		xrStringToPath(m_instance, "/user/hand/left/input/x/click", &XClickPath);
		xrStringToPath(m_instance, "/user/hand/left/input/y/touch", &YTouchPath);
		xrStringToPath(m_instance, "/user/hand/left/input/y/click", &YClickPath);
		xrStringToPath(m_instance, "/user/hand/right/input/a/touch", &ATouchPath);
		xrStringToPath(m_instance, "/user/hand/right/input/a/click", &AClickPath);
		xrStringToPath(m_instance, "/user/hand/right/input/b/touch", &BTouchPath);
		xrStringToPath(m_instance, "/user/hand/right/input/b/click", &BClickPath);
		xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuClickPath);
		xrStringToPath(m_instance, "/user/hand/right/input/system/click", &systemClickPath);

		// Suggest bindings for the Oculus Touch controllers.
		{
			XrPath oculusTouchInteractionProfilePath;
			xrStringToPath(m_instance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath);
			std::array<XrActionSuggestedBinding, VR_OPENXR_NUMINPUTBINDINGS_OCULUS - 1 /*VR_OPENXR_NUMINPUTBINDINGS_OCULUS*/> bindings{
				{/*{m_inputState.headPoseAction, headPosePath},*/
				{m_inputState.handPoseAction, handPosePath[Side_Left]},
				{m_inputState.handPoseAction, handPosePath[Side_Right]},
				{m_inputState.triggerTouchAction, triggerTouchPath[Side_Left]},
				{m_inputState.triggerTouchAction, triggerTouchPath[Side_Right]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Left]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Right]},
				{m_inputState.gripValueAction, gripValuePath[Side_Left]},
				{m_inputState.gripValueAction, gripValuePath[Side_Right]},
				{m_inputState.thumbstickXAction, thumbstickXPath[Side_Left]},
				{m_inputState.thumbstickXAction, thumbstickXPath[Side_Right]},
				{m_inputState.thumbstickYAction, thumbstickYPath[Side_Left]},
				{m_inputState.thumbstickYAction, thumbstickYPath[Side_Right]},
				{m_inputState.thumbstickTouchAction, thumbstickTouchPath[Side_Left]},
				{m_inputState.thumbstickTouchAction, thumbstickTouchPath[Side_Right]},
				{m_inputState.thumbstickClickAction, thumbstickClickPath[Side_Left]},
				{m_inputState.thumbstickClickAction, thumbstickClickPath[Side_Right]},
				{m_inputState.thumbrestTouchAction, thumbrestTouchPath[Side_Left]},
				{m_inputState.thumbrestTouchAction, thumbrestTouchPath[Side_Right]},
				{m_inputState.XTouchAction, XTouchPath},
				{m_inputState.XClickAction, XClickPath},
				{m_inputState.YTouchAction, YTouchPath},
				{m_inputState.YClickAction, YClickPath},
				{m_inputState.ATouchAction, ATouchPath},
				{m_inputState.AClickAction, AClickPath},
				{m_inputState.BTouchAction, BTouchPath},
				{m_inputState.BClickAction, BClickPath},
				{m_inputState.menuClickAction, menuClickPath},
				{m_inputState.systemClickAction, systemClickPath}} };
			XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
			suggestedBindings.suggestedBindings = &bindings[0];
			suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
			if (XR_FAILED(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings))) {
				return VR::Error_InternalFailure;
			}
		}
		break;
	}
	case HMDType_Vive:
	case HMDType_Pimax: {
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "trigger_click");
		strcpy(actionInfo.localizedActionName, "Trigger Click");
		actionInfo.countSubactionPaths = uint32_t(m_inputState.handSubactionPath.size());
		actionInfo.subactionPaths = m_inputState.handSubactionPath.data();
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trigger_value");
		strcpy(actionInfo.localizedActionName, "Trigger Value");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerValueAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "grip_click");
		strcpy(actionInfo.localizedActionName, "Grip Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.gripClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_x");
		strcpy(actionInfo.localizedActionName, "Trackpad X");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadXAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_y");
		strcpy(actionInfo.localizedActionName, "Trackpad Y");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadYAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_touch");
		strcpy(actionInfo.localizedActionName, "Trackpad Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_click");
		strcpy(actionInfo.localizedActionName, "Trackpad Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "menu_click");
		strcpy(actionInfo.localizedActionName, "Menu Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.menuClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "system_click");
		strcpy(actionInfo.localizedActionName, "System Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.systemClickAction))) {
			return VR::Error_InternalFailure;
		}
		
		XrPath headPosePath;
		std::array<XrPath, Sides> handPosePath;
		std::array<XrPath, Sides> triggerClickPath;
		std::array<XrPath, Sides> triggerValuePath;
		std::array<XrPath, Sides> gripClickPath;
		std::array<XrPath, Sides> trackpadXPath;
		std::array<XrPath, Sides> trackpadYPath;
		std::array<XrPath, Sides> trackpadTouchPath;
		std::array<XrPath, Sides> trackpadClickPath;
		std::array<XrPath, Sides> menuClickPath;
		std::array<XrPath, Sides> systemClickPath;

		xrStringToPath(m_instance, "/user/head/input/pose" /*"/user/head/pose"*/, &headPosePath);
		xrStringToPath(m_instance, "/user/hand/left/input/grip/pose", &handPosePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &handPosePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/click", &triggerClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/click", &triggerClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click", &gripClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click", &gripClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/x", &trackpadXPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/x", &trackpadXPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/y", &trackpadYPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/y", &trackpadYPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/touch", &trackpadTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/touch", &trackpadTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/click", &trackpadClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/click", &trackpadClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/system/click", &systemClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/system/click", &systemClickPath[Side_Right]);

		// Suggest bindings for the HTC Vive controllers.
		{
			XrPath viveControllerInteractionProfilePath;
			xrStringToPath(m_instance, "/interaction_profiles/htc/vive_controller", &viveControllerInteractionProfilePath);
			std::array<XrActionSuggestedBinding, VR_OPENXR_NUMINPUTBINDINGS_VIVE - 1 /*VR_OPENXR_NUMINPUTBINDINGS_VIVE*/> bindings{
				{/*{m_inputState.headPoseAction, headPosePath},*/
				{m_inputState.handPoseAction, handPosePath[Side_Left]},
				{m_inputState.handPoseAction, handPosePath[Side_Right]},
				{m_inputState.triggerTouchAction, triggerClickPath[Side_Left]},
				{m_inputState.triggerTouchAction, triggerClickPath[Side_Right]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Left]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Right]},
				{m_inputState.gripValueAction, gripClickPath[Side_Left]},
				{m_inputState.gripValueAction, gripClickPath[Side_Right]},
				{m_inputState.trackpadXAction, trackpadXPath[Side_Left]},
				{m_inputState.trackpadXAction, trackpadXPath[Side_Right]},
				{m_inputState.trackpadYAction, trackpadYPath[Side_Left]},
				{m_inputState.trackpadYAction, trackpadYPath[Side_Right]},
				{m_inputState.trackpadTouchAction, trackpadTouchPath[Side_Left]},
				{m_inputState.trackpadTouchAction, trackpadTouchPath[Side_Right]},
				{m_inputState.trackpadClickAction, trackpadClickPath[Side_Left]},
				{m_inputState.trackpadClickAction, trackpadClickPath[Side_Right]},
				{m_inputState.menuClickAction, menuClickPath[Side_Left]},
				{m_inputState.menuClickAction, menuClickPath[Side_Right]},
				{m_inputState.systemClickAction, systemClickPath[Side_Left]},
				{m_inputState.systemClickAction, systemClickPath[Side_Right]}} };
			XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
			suggestedBindings.suggestedBindings = &bindings[0];
			suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
			if (XR_FAILED(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings))) {
				return VR::Error_InternalFailure;
			}
		}
		break;
	}
	case HMDType_WindowsMR: {
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "trigger_value");
		strcpy(actionInfo.localizedActionName, "Trigger Value");
		actionInfo.countSubactionPaths = uint32_t(m_inputState.handSubactionPath.size());
		actionInfo.subactionPaths = m_inputState.handSubactionPath.data();
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerValueAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "grip_click");
		strcpy(actionInfo.localizedActionName, "Grip Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.gripClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_x");
		strcpy(actionInfo.localizedActionName, "Trackpad X");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadXAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_y");
		strcpy(actionInfo.localizedActionName, "Trackpad Y");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadYAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_touch");
		strcpy(actionInfo.localizedActionName, "Trackpad Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_click");
		strcpy(actionInfo.localizedActionName, "Trackpad Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_x");
		strcpy(actionInfo.localizedActionName, "Thumbstick X");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickXAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_y");
		strcpy(actionInfo.localizedActionName, "Thumbstick Y");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickYAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_click");
		strcpy(actionInfo.localizedActionName, "Thumbstick Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "menu_click");
		strcpy(actionInfo.localizedActionName, "Menu Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.menuClickAction))) {
			return VR::Error_InternalFailure;
		}

		XrPath headPosePath;
		std::array<XrPath, Sides> handPosePath;
		std::array<XrPath, Sides> triggerValuePath;
		std::array<XrPath, Sides> gripClickPath;
		std::array<XrPath, Sides> trackpadXPath;
		std::array<XrPath, Sides> trackpadYPath;
		std::array<XrPath, Sides> trackpadTouchPath;
		std::array<XrPath, Sides> trackpadClickPath;
		std::array<XrPath, Sides> thumbstickXPath;
		std::array<XrPath, Sides> thumbstickYPath;
		std::array<XrPath, Sides> thumbstickClickPath;
		std::array<XrPath, Sides> menuClickPath;

		xrStringToPath(m_instance, "/user/head/input/pose" /*"/user/head/pose"*/, &headPosePath);
		xrStringToPath(m_instance, "/user/hand/left/input/grip/pose", &handPosePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &handPosePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click", &gripClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click", &gripClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/x", &trackpadXPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/x", &trackpadXPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/y", &trackpadYPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/y", &trackpadYPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/touch", &trackpadTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/touch", &trackpadTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/click", &trackpadClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/click", &trackpadClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/x", &thumbstickXPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/x", &thumbstickXPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/y", &thumbstickYPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/y", &thumbstickYPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/click", &thumbstickClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuClickPath[Side_Right]);

		// Suggest bindings for the Microsoft Mixed Reality controllers.
		{
			XrPath microsoftMixedRealityInteractionProfilePath;
			xrStringToPath(m_instance, "/interaction_profiles/microsoft/motion_controller", &microsoftMixedRealityInteractionProfilePath);
			std::array<XrActionSuggestedBinding, VR_OPENXR_NUMINPUTBINDINGS_WMR - 1 /*VR_OPENXR_NUMINPUTBINDINGS_WMR*/> bindings{
				{/*{m_inputState.headPoseAction, headPosePath},*/
				{m_inputState.handPoseAction, handPosePath[Side_Left]},
				{m_inputState.handPoseAction, handPosePath[Side_Right]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Left]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Right]},
				{m_inputState.gripClickAction, gripClickPath[Side_Left]},
				{m_inputState.gripClickAction, gripClickPath[Side_Right]},
				{m_inputState.trackpadXAction, trackpadXPath[Side_Left]},
				{m_inputState.trackpadXAction, trackpadXPath[Side_Right]},
				{m_inputState.trackpadYAction, trackpadYPath[Side_Left]},
				{m_inputState.trackpadYAction, trackpadYPath[Side_Right]},
				{m_inputState.trackpadTouchAction, trackpadTouchPath[Side_Left]},
				{m_inputState.trackpadTouchAction, trackpadTouchPath[Side_Right]},
				{m_inputState.trackpadClickAction, trackpadClickPath[Side_Left]},
				{m_inputState.trackpadClickAction, trackpadClickPath[Side_Right]},
				{m_inputState.thumbstickXAction, thumbstickXPath[Side_Left]},
				{m_inputState.thumbstickXAction, thumbstickXPath[Side_Right]},
				{m_inputState.thumbstickYAction, thumbstickYPath[Side_Left]},
				{m_inputState.thumbstickYAction, thumbstickYPath[Side_Right]},
				{m_inputState.thumbstickClickAction, thumbstickClickPath[Side_Left]},
				{m_inputState.thumbstickClickAction, thumbstickClickPath[Side_Right]},
				{m_inputState.menuClickAction, menuClickPath[Side_Left]},
				{m_inputState.menuClickAction, menuClickPath[Side_Right]}} };
			XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			suggestedBindings.interactionProfile = microsoftMixedRealityInteractionProfilePath;
			suggestedBindings.suggestedBindings = &bindings[0];
			suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
			if (XR_FAILED(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings))) {
				return VR::Error_InternalFailure;
			}
		}
		break;
	}
	case HMDType_Fove: {
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "trigger_click");
		strcpy(actionInfo.localizedActionName, "Trigger Click");
		actionInfo.countSubactionPaths = uint32_t(m_inputState.handSubactionPath.size());
		actionInfo.subactionPaths = m_inputState.handSubactionPath.data();
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "menu_click");
		strcpy(actionInfo.localizedActionName, "Menu Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.menuClickAction))) {
			return VR::Error_InternalFailure;
		}

		XrPath headPosePath;
		std::array<XrPath, Sides> handPosePath;
		std::array<XrPath, Sides> triggerClickPath;
		std::array<XrPath, Sides> menuClickPath;

		xrStringToPath(m_instance, "/user/head/input/pose" /*"/user/head/pose"*/, &headPosePath);
		xrStringToPath(m_instance, "/user/hand/left/input/grip/pose", &handPosePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &handPosePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/select/click", &triggerClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/select/click", &triggerClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuClickPath[Side_Right]);

		// Suggest bindings for the Fove (Khronos simple controller).
		{
			XrPath khrSimpleControllerInteractionProfilePath;
			xrStringToPath(m_instance, "/interaction_profiles/khr/simple_controller", &khrSimpleControllerInteractionProfilePath);
			std::array<XrActionSuggestedBinding, VR_OPENXR_NUMINPUTBINDINGS_FOVE - 1 /*VR_OPENXR_NUMINPUTBINDINGS_FOVE*/> bindings{
				{/*{m_inputState.headPoseAction, headPosePath},*/
				{m_inputState.handPoseAction, handPosePath[Side_Left]},
				{m_inputState.handPoseAction, handPosePath[Side_Right]},
				{m_inputState.triggerTouchAction, triggerClickPath[Side_Left]},
				{m_inputState.triggerTouchAction, triggerClickPath[Side_Right]},
				{m_inputState.menuClickAction, menuClickPath[Side_Left]},
				{m_inputState.menuClickAction, menuClickPath[Side_Right]}} };
			XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			suggestedBindings.interactionProfile = khrSimpleControllerInteractionProfilePath;
			suggestedBindings.suggestedBindings = &bindings[0];
			suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
			if (XR_FAILED(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings))) {
				return VR::Error_InternalFailure;
			}
		}
		break;
	}
	case HMDType_Index: {
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy(actionInfo.actionName, "trigger_touch");
		strcpy(actionInfo.localizedActionName, "Trigger Touch");
		actionInfo.countSubactionPaths = uint32_t(m_inputState.handSubactionPath.size());
		actionInfo.subactionPaths = m_inputState.handSubactionPath.data();
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trigger_click");
		strcpy(actionInfo.localizedActionName, "Trigger Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trigger_value");
		strcpy(actionInfo.localizedActionName, "Trigger Value");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.triggerValueAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "grip_value");
		strcpy(actionInfo.localizedActionName, "Grip Value");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.gripValueAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "grip_force");
		strcpy(actionInfo.localizedActionName, "Grip Force");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.gripForceAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_x");
		strcpy(actionInfo.localizedActionName, "Trackpad X");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadXAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_y");
		strcpy(actionInfo.localizedActionName, "Trackpad Y");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadYAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_touch");
		strcpy(actionInfo.localizedActionName, "Trackpad Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "trackpad_force");
		strcpy(actionInfo.localizedActionName, "Trackpad Force");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.trackpadForceAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_x");
		strcpy(actionInfo.localizedActionName, "Thumbstick X");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickXAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_y");
		strcpy(actionInfo.localizedActionName, "Thumbstick Y");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickYAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_touch");
		strcpy(actionInfo.localizedActionName, "Thumbstick Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "thumbstick_click");
		strcpy(actionInfo.localizedActionName, "Thumbstick Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.thumbstickClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "A_touch");
		strcpy(actionInfo.localizedActionName, "A Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.ATouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "A_click");
		strcpy(actionInfo.localizedActionName, "A Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.AClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "B_touch");
		strcpy(actionInfo.localizedActionName, "B Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.BTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "B_click");
		strcpy(actionInfo.localizedActionName, "B Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.BClickAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "system_touch");
		strcpy(actionInfo.localizedActionName, "System Touch");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.systemTouchAction))) {
			return VR::Error_InternalFailure;
		}
		strcpy(actionInfo.actionName, "system_click");
		strcpy(actionInfo.localizedActionName, "System Click");
		if (XR_FAILED(xrCreateAction(m_inputState.actionSet, &actionInfo, &m_inputState.systemClickAction))) {
			return VR::Error_InternalFailure;
		}

		XrPath headPosePath;
		std::array<XrPath, Sides> handPosePath;
		std::array<XrPath, Sides> triggerTouchPath;
		std::array<XrPath, Sides> triggerClickPath;
		std::array<XrPath, Sides> triggerValuePath;
		std::array<XrPath, Sides> gripValuePath;
		std::array<XrPath, Sides> gripForcePath;
		std::array<XrPath, Sides> trackpadXPath;
		std::array<XrPath, Sides> trackpadYPath;
		std::array<XrPath, Sides> trackpadTouchPath;
		std::array<XrPath, Sides> trackpadForcePath;
		std::array<XrPath, Sides> thumbstickXPath;
		std::array<XrPath, Sides> thumbstickYPath;
		std::array<XrPath, Sides> thumbstickTouchPath;
		std::array<XrPath, Sides> thumbstickClickPath;
		std::array<XrPath, Sides> ATouchPath;
		std::array<XrPath, Sides> AClickPath;
		std::array<XrPath, Sides> BTouchPath;
		std::array<XrPath, Sides> BClickPath;
		std::array<XrPath, Sides> systemTouchPath;
		std::array<XrPath, Sides> systemClickPath;

		xrStringToPath(m_instance, "/user/head/input/pose" /*"/user/head/pose"*/, &headPosePath);
		xrStringToPath(m_instance, "/user/hand/left/input/grip/pose", &handPosePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &handPosePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/touch", &triggerTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/touch", &triggerTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/click", &triggerClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/click", &triggerClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value", &gripValuePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value", &gripValuePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/squeeze/force", &gripForcePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/squeeze/force", &gripForcePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/x", &trackpadXPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/x", &trackpadXPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/y", &trackpadYPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/y", &trackpadYPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/touch", &trackpadTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/touch", &trackpadTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/trackpad/force", &trackpadForcePath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/trackpad/force", &trackpadForcePath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/x", &thumbstickXPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/x", &thumbstickXPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/y", &thumbstickYPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/y", &thumbstickYPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/touch", &thumbstickTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/touch", &thumbstickTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/click", &thumbstickClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/a/touch", &ATouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/a/touch", &ATouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/a/click", &AClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/a/click", &AClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/b/touch", &BTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/b/touch", &BTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/b/click", &BClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/b/click", &BClickPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/system/touch", &systemTouchPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/system/touch", &systemTouchPath[Side_Right]);
		xrStringToPath(m_instance, "/user/hand/left/input/system/click", &systemClickPath[Side_Left]);
		xrStringToPath(m_instance, "/user/hand/right/input/system/click", &systemClickPath[Side_Right]);

		// Suggest bindings for the Valve Index controllers.
		{
			XrPath valveIndexInteractionProfilePath;
			xrStringToPath(m_instance, "/interaction_profiles/valve/index_controller", &valveIndexInteractionProfilePath);
			std::array<XrActionSuggestedBinding, VR_OPENXR_NUMINPUTBINDINGS_INDEX - 1 /*VR_OPENXR_NUMINPUTBINDINGS_INDEX*/> bindings{
				{/*{m_inputState.headPoseAction, headPosePath},*/
				{m_inputState.handPoseAction, handPosePath[Side_Left]},
				{m_inputState.handPoseAction, handPosePath[Side_Right]},
				{m_inputState.triggerTouchAction, triggerTouchPath[Side_Left]},
				{m_inputState.triggerTouchAction, triggerTouchPath[Side_Right]},
				{m_inputState.triggerClickAction, triggerClickPath[Side_Left]},
				{m_inputState.triggerClickAction, triggerClickPath[Side_Right]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Left]},
				{m_inputState.triggerValueAction, triggerValuePath[Side_Right]},
				{m_inputState.gripValueAction, gripValuePath[Side_Left]},
				{m_inputState.gripValueAction, gripValuePath[Side_Right]},
				{m_inputState.gripForceAction, gripForcePath[Side_Left]},
				{m_inputState.gripForceAction, gripForcePath[Side_Right]},
				{m_inputState.trackpadXAction, trackpadXPath[Side_Left]},
				{m_inputState.trackpadXAction, trackpadXPath[Side_Right]},
				{m_inputState.trackpadYAction, trackpadYPath[Side_Left]},
				{m_inputState.trackpadYAction, trackpadYPath[Side_Right]},
				{m_inputState.trackpadTouchAction, trackpadTouchPath[Side_Left]},
				{m_inputState.trackpadTouchAction, trackpadTouchPath[Side_Right]},
				{m_inputState.trackpadForceAction, trackpadForcePath[Side_Left]},
				{m_inputState.trackpadForceAction, trackpadForcePath[Side_Right]},
				{m_inputState.thumbstickXAction, thumbstickXPath[Side_Left]},
				{m_inputState.thumbstickXAction, thumbstickXPath[Side_Right]},
				{m_inputState.thumbstickYAction, thumbstickYPath[Side_Left]},
				{m_inputState.thumbstickYAction, thumbstickYPath[Side_Right]},
				{m_inputState.thumbstickTouchAction, thumbstickTouchPath[Side_Left]},
				{m_inputState.thumbstickTouchAction, thumbstickTouchPath[Side_Right]},
				{m_inputState.thumbstickClickAction, thumbstickClickPath[Side_Left]},
				{m_inputState.thumbstickClickAction, thumbstickClickPath[Side_Right]},
				{m_inputState.ATouchAction, ATouchPath[Side_Left]},
				{m_inputState.ATouchAction, ATouchPath[Side_Right]},
				{m_inputState.AClickAction, AClickPath[Side_Left]},
				{m_inputState.AClickAction, AClickPath[Side_Right]},
				{m_inputState.BTouchAction, BTouchPath[Side_Left]},
				{m_inputState.BTouchAction, BTouchPath[Side_Right]},
				{m_inputState.BClickAction, BClickPath[Side_Left]},
				{m_inputState.BClickAction, BClickPath[Side_Right]},
				{m_inputState.systemTouchAction, systemTouchPath[Side_Left]},
				{m_inputState.systemTouchAction, systemTouchPath[Side_Right]},
				{m_inputState.systemClickAction, systemClickPath[Side_Left]},
				{m_inputState.systemClickAction, systemClickPath[Side_Right]}} };
			XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
			suggestedBindings.interactionProfile = valveIndexInteractionProfilePath;
			suggestedBindings.suggestedBindings = &bindings[0];
			suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
			if (XR_FAILED(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings))) {
				return VR::Error_InternalFailure;
			}
		}
		break;
	}
	default: {
		// Unsupported HMD type
		return VR::Error_InvalidParameter;
	}
	}

	XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &m_inputState.actionSet;
	if (XR_FAILED(xrAttachSessionActionSets(m_session, &attachInfo))) {
		return VR::Error_InternalFailure;
	}

	XrActionSpaceCreateInfo actionSpaceInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
	actionSpaceInfo.action = m_inputState.headPoseAction;
	actionSpaceInfo.subactionPath = m_inputState.headSubactionPath;
	actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
	if (XR_FAILED(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_inputState.headSpace))) {
		return VR::Error_InternalFailure;
	}

	actionSpaceInfo.action = m_inputState.handPoseAction;
	actionSpaceInfo.subactionPath = m_inputState.handSubactionPath[Side_Left];
	if (XR_FAILED(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_inputState.handSpace[Side_Left]))) {
		return VR::Error_InternalFailure;
	}
	actionSpaceInfo.subactionPath = m_inputState.handSubactionPath[Side_Right];
	if (XR_FAILED(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_inputState.handSpace[Side_Right]))) {
		return VR::Error_InternalFailure;
	}

	// Create reference space
	XrReferenceSpaceCreateInfo refspace_info{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	refspace_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	refspace_info.poseInReferenceSpace.position = { 0.0f, 0.0f, 0.0f };
	refspace_info.poseInReferenceSpace.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
	if (XR_FAILED(xrCreateReferenceSpace(m_session, &refspace_info, &m_appSpace))) {
		return VR::Error_InternalFailure;
	}

	// Start session
	XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
	sessionBeginInfo.primaryViewConfigurationType = m_viewConfigType;
	if (XR_FAILED(xrBeginSession(m_session, &sessionBeginInfo))) {
		return VR::Error_InternalFailure;
	}

	// Initialize views data
	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	uint32_t viewCapacityInput = (uint32_t)m_views.size();
	uint32_t viewCountOutput;

	XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
	xrWaitFrame(m_session, &frameWaitInfo, &m_frameState);

	XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	viewLocateInfo.displayTime = m_frameState.predictedDisplayTime;
	viewLocateInfo.space = m_appSpace;
	if (XR_FAILED(xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data()))) {
		return VR::Error_InternalFailure;
	}

	// Get head position data if not set manually
	/* TODO_XR */
	/*if (!this->eye_offset_override[Side_Left]) {
		transferHMDTransformation()
		t_hmd2eye[Side_Left][0][0] = m.m[0][0];  t_hmd2eye[Side_Left][1][0] = m.m[0][1];  t_hmd2eye[Side_Left][2][0] = m.m[0][2];  t_hmd2eye[Side_Left][3][0] = m.m[0][3];
		t_hmd2eye[Side_Left][0][1] = m.m[1][0];  t_hmd2eye[Side_Left][1][1] = m.m[1][1];  t_hmd2eye[Side_Left][2][1] = m.m[1][2];  t_hmd2eye[Side_Left][3][1] = m.m[1][3];
		t_hmd2eye[Side_Left][0][2] = m.m[2][0];  t_hmd2eye[Side_Left][1][2] = m.m[2][1];  t_hmd2eye[Side_Left][2][2] = m.m[2][2];  t_hmd2eye[Side_Left][3][2] = m.m[2][3];
		t_hmd2eye[Side_Left][0][3] = 0;          t_hmd2eye[Side_Left][1][3] = 0;          t_hmd2eye[Side_Left][2][3] = 0;          t_hmd2eye[Side_Left][3][3] = 1;
	}
	if (!this->eye_offset_override[Side_Right]) {
		transferHMDTransformation()
		t_hmd2eye[Side_Right][0][0] = m.m[0][0];  t_hmd2eye[Side_Right][1][0] = m.m[0][1];  t_hmd2eye[Side_Right][2][0] = m.m[0][2];  t_hmd2eye[Side_Right][3][0] = m.m[0][3];
		t_hmd2eye[Side_Right][0][1] = m.m[1][0];  t_hmd2eye[Side_Right][1][1] = m.m[1][1];  t_hmd2eye[Side_Right][2][1] = m.m[1][2];  t_hmd2eye[Side_Right][3][1] = m.m[1][3];
		t_hmd2eye[Side_Right][0][2] = m.m[2][0];  t_hmd2eye[Side_Right][1][2] = m.m[2][1];  t_hmd2eye[Side_Right][2][2] = m.m[2][2];  t_hmd2eye[Side_Right][3][2] = m.m[2][3];
		t_hmd2eye[Side_Right][0][3] = 0;          t_hmd2eye[Side_Right][1][3] = 0;          t_hmd2eye[Side_Right][2][3] = 0;          t_hmd2eye[Side_Right][3][3] = 1;
	}*/

	this->initialized = true;

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/		uninit()
/**
 * Uninitialize the VR module.
 */
int VR_OpenXR::uninit()
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

	xrEndSession(m_session);

	// save current context so that we can return
#ifdef _WIN32
	HDC   dc = wglGetCurrentDC();
	HGLRC rc = wglGetCurrentContext();
	// switch to the context we had when initializing
	if (rc != this->gl.context) {
		wglMakeCurrent(this->gl.device, this->gl.context);
	}

#if XR_USE_GRAPHICS_API_D3D11
	// Destroy render buffers
	for (int i = 0; i < Sides; ++i) {
		/* TODO_XR: This causes crash on AMD Vega64. */
		if (shared_device && shared_texture[i]) {
			wglDXUnregisterObjectNV(shared_device, shared_texture[i]);
		}
		shared_texture[i] = NULL;
	}
	if (shared_device) {
		wglDXCloseDeviceNV(shared_device);
		shared_device = NULL;
	}
	this->d3d.release();
#endif
	this->gl.release();

	this->releaseHMD();

	// return to previous context
	if (rc != this->gl.context) {
		wglMakeCurrent(dc, rc);
	}
#else
	Display* display = glXGetCurrentDisplay();
	GLXDrawable drawable = glXGetCurrentDrawable();
	GLXContext context = glXGetCurrentContext();
	// switch to the context we had when initializing
	if (context != this->gl.context) {
		glXMakeCurrent(this->gl.display, this->gl.drawable, this->gl.context);
	}

	// Destroy render buffers
	this->gl.release();

	this->releaseHMD();

	// return to previous context
	if (context != this->gl.context) {
		glXMakeCurrent(display, drawable, context);
	}
#endif

	if (m_inputState.actionSet != XR_NULL_HANDLE) {
		xrDestroySpace(m_inputState.headSpace);
		for (int i = 0; i < Sides; ++i) {
			xrDestroySpace(m_inputState.handSpace[i]);
		}
		xrDestroyActionSet(m_inputState.actionSet);
		m_inputState.actionSet = { XR_NULL_HANDLE };
	}

	for (Swapchain swapchain : m_swapchains) {
		xrDestroySwapchain(swapchain.handle);
	}
	m_swapchains.clear();
	m_swapchainImageBuffers.clear();

	if (m_appSpace != XR_NULL_HANDLE) {
		xrDestroySpace(m_appSpace);
		m_appSpace = XR_NULL_HANDLE;
	}

	if (m_session != XR_NULL_HANDLE) {
		xrDestroySession(m_session);
		m_session = XR_NULL_HANDLE;
	}

	if (m_instance != XR_NULL_HANDLE) {
		xrDestroyInstance(m_instance);
		m_instance = XR_NULL_HANDLE;
	}

	this->initialized = false;
	return VR::Error_None;
}

//                                                                      ____________________________
//_____________________________________________________________________/ transferHMDTransformation()
/**
 * Helper function to transform OpenXR transformations into BlenderXR transformation matrices.
 * \param   pos     Pose / tracking information.
 * \param   m       [OUT] Transformation matrix (both translation and rotation).
 */
static void transferHMDTransformation(const XrPosef& pose, float m[4][4])
{
	const XrVector3f& p = pose.position;
	const XrQuaternionf& q = pose.orientation;
	m[0][0] = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
	m[1][0] = 2 * q.x * q.y - 2 * q.z * q.w;
	m[2][0] = 2 * q.x * q.z + 2 * q.y * q.w;
	m[3][0] = p.x;
	m[0][1] = -(2 * q.x * q.z - 2 * q.y * q.w);
	m[1][1] = -(2 * q.y * q.z + 2 * q.x * q.w);
	m[2][1] = -(1 - 2 * q.x * q.x - 2 * q.y * q.y);
	m[3][1] = -p.z;
	m[0][2] = 2 * q.x * q.y + 2 * q.z * q.w;
	m[1][2] = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
	m[2][2] = 2 * q.y * q.z - 2 * q.x * q.w;
	m[3][2] = p.y;
	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = 1;
}

//                                                                      ____________________________
//_____________________________________________________________________/ transferControllerTransformation()
/**
 * Helper function to transform OpenXR transformations into BlenderXR transformation matrices.
 * \param   pos     Pose / tracking information.
 * \param   m       [OUT] Transformation matrix (both translation and rotation).
 */
static void transferControllerTransformation(const XrPosef& pose, float m[4][4])
{
	const XrVector3f& p = pose.position;
	const XrQuaternionf& q = pose.orientation;
	// x-axis
	m[0][0] = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
	m[0][1] = -(2 * q.x * q.z - 2 * q.y * q.w);
	m[0][2] = 2 * q.x * q.y + 2 * q.z * q.w;
	// y-axis
	m[1][0] = -(2 * q.x * q.z + 2 * q.y * q.w);
	m[1][1] = (1 - 2 * q.x * q.x - 2 * q.y * q.y);
	m[1][2] = -(2 * q.y * q.z - 2 * q.x * q.w);
	// z-axis
	m[2][0] = 2 * q.x * q.y - 2 * q.z * q.w;
	m[2][1] = -(2 * q.y * q.z + 2 * q.x * q.w);
	m[2][2] = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
	// translation (moved ahead 50mm)
	m[3][0] = p.x + (0.05f * m[1][0]);
	m[3][1] = -p.z + (0.05f * m[1][1]);
	m[3][2] = p.y + (0.05f * m[1][2]);
	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = 1;
}

//                                                                          ________________________
//_________________________________________________________________________/  updateTracking()
/**
 * Update the t_eye positions based on latest tracking data.
 * \return      Zero on success, an error code on failure.
 */
int VR_OpenXR::updateTracking()
{
	if (!m_instance || !m_session) {
		return VR::Error_NotInitialized;
	}

	// Assume tracking was lost
	this->tracking = false;
	
	/* TODO_XR */
	// Just count up base stations - the order should be the same
	//uint base_station_index = 0;

	// Process latest tracking data
	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	uint32_t viewCapacityInput = (uint32_t)m_views.size();
	uint32_t viewCountOutput;

	/*XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
	XrFrameState frameState{ XR_TYPE_FRAME_STATE };
	xrWaitFrame(m_session, &frameWaitInfo, &frameState);*/

	XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	viewLocateInfo.displayTime = m_frameState.predictedDisplayTime;
	viewLocateInfo.space = m_appSpace;
	if (XR_FAILED(xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data()))) {
		return VR::Error_InternalFailure;
	}

	// Sync action data
	XrActiveActionSet activeActionSet{ m_inputState.actionSet, XR_NULL_PATH };
	XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	if (XR_FAILED(xrSyncActions(m_session, &syncInfo))) {
		return VR::Error_InternalFailure;
	}
	this->tracking = true;

	XrSpaceLocation spaceLocation{ XR_TYPE_SPACE_LOCATION, XR_NULL_PATH };
	//if (XR_FAILED(xrLocateSpace(m_appSpace, m_appSpace, m_frameState.predictedDisplayTime, &spaceLocation))) {
	//	return VR::Error_InternalFailure;
	//}

	// Eyes
	for (int i = 0; i < Sides; ++i) {
		transferHMDTransformation(m_views[i].pose, this->t_eye[i]);
	}
	// HMD
	XrActionStatePose poseState{ XR_TYPE_ACTION_STATE_POSE };
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
#if 0
	/* TODO_XR */
	activeActionSet.subactionPath = m_inputState.headSubactionPath;
	if (!XR_FAILED(xrSyncActions(m_session, &syncInfo))) {
		xrGetActionStatePose(m_session, &getInfo, &poseState);
		if (poseState.isActive && !XR_FAILED(xrLocateSpace(m_inputState.headSpace, m_appSpace, m_frameState.predictedDisplayTime, &spaceLocation))) {
			transferHMDTransformation(spaceLocation.pose, this->t_hmd);
		}
	}
#else
	// Temp workaround: Average eye transforms...
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			this->t_hmd[i][j] = (this->t_eye[Side_Left][i][j] + this->t_eye[Side_Right][i][j]) / 2.0f;
		}
	}
#endif

	// Controllers
	getInfo.action = m_inputState.handPoseAction;
	for (int i = 0; i < Sides; ++i) {
		activeActionSet.subactionPath = m_inputState.handSubactionPath[i];
		if (!XR_FAILED(xrSyncActions(m_session, &syncInfo))) {
			getInfo.subactionPath = m_inputState.handSubactionPath[i];
			xrGetActionStatePose(m_session, &getInfo, &poseState);
			if (poseState.isActive && !XR_FAILED(xrLocateSpace(m_inputState.handSpace[i], m_appSpace, m_frameState.predictedDisplayTime, &spaceLocation))) {
				transferControllerTransformation(spaceLocation.pose, this->t_controller[i]);
				this->interpretControllerState(this->t_controller[i], this->controller[i]);
			}
		}
	}

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getTrackerPosition()
/**
 * Get the position of a tracking camera / device (if available).
 * \i           Index of the camera tracking device to query.
 * \t           [OUT] Matrix to receive the transformation.
 * \return      Zero on success, an error code on failure.
 */
int VR_OpenXR::getTrackerPosition(uint i, float t[4][4]) const
{
	if (i >= VR_OPENXR_NUMBASESTATIONS) {
		return VR:: Error_InvalidParameter;
	}
	memcpy(t, this->t_basestation[i], sizeof(float) * 4 * 4);
	return VR_OpenXR::Error_None;
}

//                                                                      ____________________________
//_____________________________________________________________________/ interpretControllerState()
/**
 * Helper function to deal with VR controller data.
 */
void VR_OpenXR::interpretControllerState( float t_controller[4][4], Controller& c)
{
	c.available = 1;
	
	// offset, so that the cursor is ahead of the controller
	// for HTC Vive contollers, the offset should be 60.0 mm
	// for WindowsMR controllers, the offset should be 30.0 mm
	float controller_offset;
	switch (this->hmd_type) {
	case HMDType_Vive: 
	case HMDType_Pimax: {
		controller_offset = 0.06f;
		break;
	}
	case HMDType_WindowsMR: {
		controller_offset = 0.03f;
		break;
	}
	default: {
		controller_offset = 0.0f;
		break;
	}
	}
	t_controller[3][0] += t_controller[1][0] * controller_offset;
	t_controller[3][1] += t_controller[1][1] * controller_offset;
	t_controller[3][2] += t_controller[1][2] * controller_offset;

	clock_t now = clock();

	uint64_t prior_touchpad_pressed = c.buttons & VR_OPENXR_BTNBITS_DPADANY;

	// Convert OpenXR button bits to Widget_Layout button bits.
	c.buttons = c.buttons_touched = 0;

	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
	getInfo.subactionPath = m_inputState.handSubactionPath[c.side];
	XrActionStateFloat inputValue{ XR_TYPE_ACTION_STATE_FLOAT };

	switch (this->hmd_type) {
	case HMDType_Oculus: {
		if (c.side == Side_Left) {
			getInfo.action = m_inputState.XTouchAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
				(inputValue.currentState > 0.0f)) {
				c.buttons_touched |= VR_OPENXR_BTNBIT_X;
				getInfo.action = m_inputState.XClickAction;
				if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
					(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
					c.buttons |= VR_OPENXR_BTNBIT_X;
				}
			}
			getInfo.action = m_inputState.YTouchAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > 0.0f)) {
				c.buttons_touched |= VR_OPENXR_BTNBIT_Y;
				getInfo.action = m_inputState.YClickAction;
				if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
					(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
					c.buttons |= VR_OPENXR_BTNBIT_Y;
				}
			}
			getInfo.action = m_inputState.menuClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > 0.0f)) {
				c.buttons_touched |= VR_OPENXR_BTNBIT_MENU;
				if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
					c.buttons |= VR_OPENXR_BTNBIT_MENU;
				}
			}
		}
		else
		{
			getInfo.action = m_inputState.ATouchAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > 0.0f)) {
				c.buttons_touched |= VR_OPENXR_BTNBIT_A;
				getInfo.action = m_inputState.AClickAction;
				if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
					(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
					c.buttons |= VR_OPENXR_BTNBIT_A;
				}
			}
			getInfo.action = m_inputState.BTouchAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > 0.0f)) {
				c.buttons_touched |= VR_OPENXR_BTNBIT_B;
				getInfo.action = m_inputState.BClickAction;
				if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
					(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
					c.buttons |= VR_OPENXR_BTNBIT_B;
				}
			}
			getInfo.action = m_inputState.systemClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > 0.0f)) {
				c.buttons_touched |= VR_OPENXR_BTNBIT_SYSTEM;
				if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
					c.buttons |= VR_OPENXR_BTNBIT_SYSTEM;
				}
			}
		}
		getInfo.action = m_inputState.thumbrestTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTTHUMBREST :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTTHUMBREST;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTTHUMBREST :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTTHUMBREST;
			}
		}

		// Trigger:
		// override the button with our own trigger pressure threshold
		c.trigger_pressure = 0;
		getInfo.action = m_inputState.triggerTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
			getInfo.action = m_inputState.triggerValueAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_TRIGGERPRESSURETHRESHOLD)) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
				// map pressure to 0~1 for everything above the threshold
				c.trigger_pressure = (inputValue.currentState - VR_OPENXR_TRIGGERPRESSURETHRESHOLD) / (1.0f - VR_OPENXR_TRIGGERPRESSURETHRESHOLD);
			}
		}
		// Grip:
		// override the button with our own grip pressure threshold
		c.grip_pressure = 0;
		getInfo.action = m_inputState.gripValueAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTGRIP :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTGRIP;
			if (inputValue.currentState > VR_OPENXR_GRIPPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTGRIP :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTGRIP;
				// map pressure to 0~1 for everything above the threshold
				c.grip_pressure = (inputValue.currentState - VR_OPENXR_GRIPPRESSURETHRESHOLD) / (1.0f - VR_OPENXR_GRIPPRESSURETHRESHOLD);
			}
		}

		// Thumbstick
		getInfo.action = m_inputState.thumbstickTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_STICKLEFT :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_STICKRIGHT;
			getInfo.action = m_inputState.thumbstickClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_STICKLEFT :
									  c.buttons |= VR_OPENXR_BTNBIT_STICKRIGHT;
			}
		}
		getInfo.action = m_inputState.thumbstickXAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
			c.stick[0] = inputValue.currentState;
		}
		getInfo.action = m_inputState.thumbstickYAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
			c.stick[1] = inputValue.currentState;
		}

		const float& stickX = c.stick[0];
		const float& stickY = c.stick[1];
		if (stickX != 0.0f || stickY != 0.0f) {
			if (std::abs(stickX) > std::abs(stickY)) { // LEFT or RIGHT
				if (stickX > VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKRIGHT;
					if (stickX > VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKRIGHT;
					}
				}
				else if (stickX < -VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKLEFT;
					if (stickX < -VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKLEFT;
					}
				} // else: center
			}
			else { // UP or DOWN
				if (stickY > VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION * 0.7f) { // UP (reduced threshold, because it's hard to hit)
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKUP;
					if (stickY > VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION * 0.7f) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKUP;
					}
				}
				else if (stickY < -VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKDOWN;
					if (stickY < -VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKDOWN;
					}
				} // else: center
			}
		}
		break;
	}
	case HMDType_Vive:
	case HMDType_WindowsMR: 
	case HMDType_Pimax: {
		getInfo.action = m_inputState.gripClickAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTGRIP :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTGRIP;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTGRIP :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTGRIP;
			}
		}
		getInfo.action = m_inputState.menuClickAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
			(inputValue.currentState > 0.0f)) {
			c.buttons_touched |= VR_OPENXR_BTNBIT_MENU;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.buttons |= VR_OPENXR_BTNBIT_MENU;
			}
		}
		getInfo.action = m_inputState.systemClickAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
			(inputValue.currentState > 0.0f)) {
			c.buttons_touched |= VR_OPENXR_BTNBIT_SYSTEM;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.buttons |= VR_OPENXR_BTNBIT_SYSTEM;
			}
		}

		// Trigger:
		// override the button with our own trigger pressure threshold
		c.trigger_pressure = 0;
		getInfo.action = m_inputState.triggerValueAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
			if (inputValue.currentState > VR_OPENXR_TRIGGERPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
				// map pressure to 0~1 for everything above the threshold
				c.trigger_pressure = (inputValue.currentState - VR_OPENXR_TRIGGERPRESSURETHRESHOLD) / (1.0f - VR_OPENXR_TRIGGERPRESSURETHRESHOLD);
			}
		}

		// Touchpad
		bool touchpad_touched = false;
		bool touchpad_pressed = false;
		getInfo.action = m_inputState.trackpadTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			touchpad_touched = true;
			getInfo.action = m_inputState.trackpadClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				touchpad_pressed = true;
			}
		}

		if (touchpad_touched) {
			getInfo.action = m_inputState.trackpadXAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
				c.dpad[0] = inputValue.currentState;
			}
			getInfo.action = m_inputState.trackpadYAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
				c.dpad[1] = inputValue.currentState;
			}
		}

		// Convert touchpad position to button
		static ui64 touchpad_btn[2] = { 0,0 }; // static, so that it stays on the last button until I move on some other button
		if (c.dpad[0] != 0.0f || c.dpad[1] != 0.0f) {
			if (std::abs(c.dpad[0]) > std::abs(c.dpad[1])) { // LEFT or RIGHT
				if (c.dpad[0] > VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD) { // RIGHT
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADRIGHT;
				}
				else if (c.dpad[0] < -VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD) { // LEFT
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADLEFT;
				}
				else { // CENTER
					c.side == Side_Left ? touchpad_btn[c.side] = VR_OPENXR_BTNBIT_LEFTDPAD :
										  touchpad_btn[c.side] = VR_OPENXR_BTNBIT_RIGHTDPAD;
				}
			}
			else { // UP or DOWN
				if (c.dpad[1] > 0.05f) { // UP (reduced threshold, because it's hard to hit)
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADUP;
				}
				else if (c.dpad[1] < -VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD) { // DOWN
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADDOWN;
				}
				else { // CENTER
					c.side == Side_Left ? touchpad_btn[c.side] = VR_OPENXR_BTNBIT_LEFTDPAD :
										  touchpad_btn[c.side] = VR_OPENXR_BTNBIT_RIGHTDPAD;
				}
			}
		}

		// Touchpad touch:
		static clock_t prior_touch_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
		if (touchpad_touched || (now - prior_touch_touchpad[c.side]) < VR_OPENXR_DEBOUNCEPERIOD) {
			// if we're pressing a button, we stick with that until we let go
			if (prior_touchpad_pressed) {
				c.buttons_touched |= prior_touchpad_pressed;
			}
			else {
				c.buttons_touched |= touchpad_btn[c.side];
			}
			if (touchpad_touched) {
				prior_touch_touchpad[c.side] = now;
			}
		}

		// Touchpad press:
		// When the touchpad is pressed, find out in what quadrant (left, right, up, down)
		static clock_t prior_press_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
		if (touchpad_pressed || (now - prior_press_touchpad[c.side]) < VR_OPENXR_DEBOUNCEPERIOD) {
			// if we already are pressing one of the buttons, continue using it,
			if (prior_touchpad_pressed) {
				c.buttons |= prior_touchpad_pressed;
			}
			else {
				c.buttons |= touchpad_btn[c.side];
			}
			if (touchpad_pressed) {
				prior_press_touchpad[c.side] = now;
			}
		}

		if (this->hmd_type != HMDType_WindowsMR) {
			return;
		}

		// Thumbstick
		getInfo.action = m_inputState.thumbstickClickAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) && 
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_STICKLEFT :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_STICKRIGHT;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_STICKLEFT :
									  c.buttons |= VR_OPENXR_BTNBIT_STICKRIGHT;
			}
		}
		getInfo.action = m_inputState.thumbstickXAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
			c.stick[0] = inputValue.currentState;
		}
		getInfo.action = m_inputState.thumbstickYAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
			c.stick[1] = inputValue.currentState;
		}

		const float& stickX = c.stick[0];
		const float& stickY = c.stick[1];
		if (stickX != 0.0f || stickY != 0.0f) {
			if (std::abs(stickX) > std::abs(stickY)) { // LEFT or RIGHT
				if (stickX > VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKRIGHT;
					if (stickX > VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKRIGHT;
					}
				}
				else if (stickX < -VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKLEFT;
					if (stickX < -VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKLEFT;
					}
				} // else: center
			}
			else { // UP or DOWN
				if (stickY > VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION * 0.7f) { // UP (reduced threshold, because it's hard to hit)
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKUP;
					if (stickY > VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION * 0.7f) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKUP;
					}
				}
				else if (stickY < -VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKDOWN;
					if (stickY < -VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKDOWN;
					}
				} // else: center
			}
		}
		break;
	}
	case HMDType_Fove: {
		getInfo.action = m_inputState.triggerClickAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
			}
		}
		getInfo.action = m_inputState.menuClickAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.buttons_touched |= VR_OPENXR_BTNBIT_MENU;
			if (inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD) {
				c.buttons |= VR_OPENXR_BTNBIT_MENU;
			}
		}
		break;
	}
	case HMDType_Index: {
		getInfo.action = m_inputState.ATouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.buttons_touched |= VR_OPENXR_BTNBIT_A;
			getInfo.action = m_inputState.AClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				c.buttons |= VR_OPENXR_BTNBIT_A;
			}
		}
		getInfo.action = m_inputState.BTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.buttons_touched |= VR_OPENXR_BTNBIT_B;
			getInfo.action = m_inputState.BClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				c.buttons |= VR_OPENXR_BTNBIT_B;
			}
		}
		getInfo.action = m_inputState.systemTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.buttons_touched |= VR_OPENXR_BTNBIT_SYSTEM;
			getInfo.action = m_inputState.systemClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				c.buttons |= VR_OPENXR_BTNBIT_SYSTEM;
			}
		}


		// Trigger:
		// override the button with our own trigger pressure threshold
		c.trigger_pressure = 0;
		getInfo.action = m_inputState.triggerTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
			getInfo.action = m_inputState.triggerValueAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_TRIGGERPRESSURETHRESHOLD)) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTTRIGGER :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTTRIGGER;
				// map pressure to 0~1 for everything above the threshold
				c.trigger_pressure = (inputValue.currentState - VR_OPENXR_TRIGGERPRESSURETHRESHOLD) / (1.0f - VR_OPENXR_TRIGGERPRESSURETHRESHOLD);
			}
		}
		// Grip:
		// override the button with our own grip pressure threshold
		c.grip_pressure = 0;
		getInfo.action = m_inputState.gripValueAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_LEFTGRIP :
								  c.buttons_touched |= VR_OPENXR_BTNBIT_RIGHTGRIP;
			if (inputValue.currentState > VR_OPENXR_GRIPPRESSURETHRESHOLD) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_LEFTGRIP :
									  c.buttons |= VR_OPENXR_BTNBIT_RIGHTGRIP;
				// map pressure to 0~1 for everything above the threshold
				c.grip_pressure = (inputValue.currentState - VR_OPENXR_GRIPPRESSURETHRESHOLD) / (1.0f - VR_OPENXR_GRIPPRESSURETHRESHOLD);
			}
		}

		// Touchpad
		bool touchpad_touched = false;
		bool touchpad_pressed = false;
		getInfo.action = m_inputState.trackpadTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			touchpad_touched = true;
			getInfo.action = m_inputState.trackpadForceAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				touchpad_pressed = true;
			}
		}

		if (touchpad_touched) {
			getInfo.action = m_inputState.trackpadXAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
				c.dpad[0] = inputValue.currentState;
			}
			getInfo.action = m_inputState.trackpadYAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
				c.dpad[1] = inputValue.currentState;
			}
		}

		// Convert touchpad position to button
		static ui64 touchpad_btn[2] = { 0,0 }; // static, so that it stays on the last button until I move on some other button
		if (c.dpad[0] != 0.0f || c.dpad[1] != 0.0f) {
			if (std::abs(c.dpad[0]) > std::abs(c.dpad[1])) { // LEFT or RIGHT
				if (c.dpad[0] > VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD) { // RIGHT
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADRIGHT;
				}
				else if (c.dpad[0] < -VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD) { // LEFT
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADLEFT;
				}
				else { // CENTER
					c.side == Side_Left ? touchpad_btn[c.side] = VR_OPENXR_BTNBIT_LEFTDPAD :
						touchpad_btn[c.side] = VR_OPENXR_BTNBIT_RIGHTDPAD;
				}
			}
			else { // UP or DOWN
				if (c.dpad[1] > 0.05f) { // UP (reduced threshold, because it's hard to hit)
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADUP;
				}
				else if (c.dpad[1] < -VR_OPENXR_TRACKPADDIRECTIONTHRESHOLD) { // DOWN
					touchpad_btn[c.side] = VR_OPENXR_BTNBIT_DPADDOWN;
				}
				else { // CENTER
					c.side == Side_Left ? touchpad_btn[c.side] = VR_OPENXR_BTNBIT_LEFTDPAD :
						touchpad_btn[c.side] = VR_OPENXR_BTNBIT_RIGHTDPAD;
				}
			}
		}

		// Touchpad touch:
		static clock_t prior_touch_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
		if (touchpad_touched || (now - prior_touch_touchpad[c.side]) < VR_OPENXR_DEBOUNCEPERIOD) {
			// if we're pressing a button, we stick with that until we let go
			if (prior_touchpad_pressed) {
				c.buttons_touched |= prior_touchpad_pressed;
			}
			else {
				c.buttons_touched |= touchpad_btn[c.side];
			}
			if (touchpad_touched) {
				prior_touch_touchpad[c.side] = now;
			}
		}

		// Touchpad press:
		// When the touchpad is pressed, find out in what quadrant (left, right, up, down)
		static clock_t prior_press_touchpad[2] = { 0, 0 }; // for smoothing micro touch-losses
		if (touchpad_pressed || (now - prior_press_touchpad[c.side]) < VR_OPENXR_DEBOUNCEPERIOD) {
			// if we already are pressing one of the buttons, continue using it,
			if (prior_touchpad_pressed) {
				c.buttons |= prior_touchpad_pressed;
			}
			else {
				c.buttons |= touchpad_btn[c.side];
			}
			if (touchpad_pressed) {
				prior_press_touchpad[c.side] = now;
			}
		}

		// Thumbstick
		getInfo.action = m_inputState.thumbstickTouchAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
			(inputValue.currentState > 0.0f)) {
			c.side == Side_Left ? c.buttons_touched |= VR_OPENXR_BTNBIT_STICKLEFT :
				c.buttons_touched |= VR_OPENXR_BTNBIT_STICKRIGHT;
			getInfo.action = m_inputState.thumbstickClickAction;
			if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue)) &&
				(inputValue.currentState > VR_OPENXR_BUTTONPRESSURETHRESHOLD)) {
				c.side == Side_Left ? c.buttons |= VR_OPENXR_BTNBIT_STICKLEFT :
									  c.buttons |= VR_OPENXR_BTNBIT_STICKRIGHT;
			}
		}
		getInfo.action = m_inputState.thumbstickXAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
			c.stick[0] = inputValue.currentState;
		}
		getInfo.action = m_inputState.thumbstickYAction;
		if (!XR_FAILED(xrGetActionStateFloat(m_session, &getInfo, &inputValue))) {
			c.stick[1] = inputValue.currentState;
		}

		const float& stickX = c.stick[0];
		const float& stickY = c.stick[1];
		if (stickX != 0.0f || stickY != 0.0f) {
			if (std::abs(stickX) > std::abs(stickY)) { // LEFT or RIGHT
				if (stickX > VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // RIGHT
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKRIGHT;
					if (stickX > VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKRIGHT;
					}
				}
				else if (stickX < -VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // LEFT
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKLEFT;
					if (stickX < -VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKLEFT;
					}
				} // else: center
			}
			else { // UP or DOWN
				if (stickY > VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION * 0.7f) { // UP (reduced threshold, because it's hard to hit)
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKUP;
					if (stickY > VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION * 0.7f) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKUP;
					}
				}
				else if (stickY < -VR_OPENXR_TOUCHTHRESHOLD_STICKDIRECTION) { // DOWN
					c.buttons_touched |= VR_OPENXR_BTNBIT_STICKDOWN;
					if (stickY < -VR_OPENXR_PRESSTHRESHOLD_STICKDIRECTION) { // "PRESS"
						c.buttons |= VR_OPENXR_BTNBIT_STICKDOWN;
					}
				} // else: center
			}
		}
		break;
	}
	default: {
		// Unsupported HMD type
		return;
	}
	}
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEye()
/**
 * Blit a rendered image into the internal eye texture.
  * TODO_XR: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_OpenXR::blitEye(Side side, void* texture_resource, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	wglDXLockObjectsNV(shared_device, 1, &shared_texture[side]);
#endif
#endif

	uint texture_id = *((uint*)texture_resource);

	// Save previous OpenGL state
	GLint prior_framebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prior_framebuffer);
	GLint prior_program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prior_program);
	GLboolean prior_backface_culling = glIsEnabled(GL_CULL_FACE);
	GLboolean prior_blend_enabled = glIsEnabled(GL_BLEND);
	GLboolean prior_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean prior_texture_enabled = glIsEnabled(GL_TEXTURE_2D);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	glUseProgram(this->gl.program);

	// Bind my render buffer as render target eye render target
	glBindFramebuffer(GL_FRAMEBUFFER, this->gl.framebuffer[side]);
	glViewport(0, 0, this->texture_width, this->texture_height);
	glClearColor(0, 0, 0, 0);
	/* TODO_XR: Clearing the color buffer bit causes crash on AMD Vega64. */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0f / this->gamma, 0);

	// Render the provided texture into the hmd eye texture.
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glBindVertexArray(this->gl.vertex_array);
	glEnableVertexAttribArray(this->gl.position_location);
	glEnableVertexAttribArray(this->gl.uv_location);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(this->gl.position_location);
	glDisableVertexAttribArray(this->gl.uv_location);

	// Restore previous OpenGL state
	glUseProgram(prior_program);
	prior_backface_culling ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
	prior_blend_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	prior_depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
	prior_texture_enabled ? glEnable(GL_TEXTURE_2D) : glDisable(GL_TEXTURE_2D);
	glBindFramebuffer(GL_FRAMEBUFFER, prior_framebuffer);

#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	wglDXUnlockObjectsNV(shared_device, 1, &shared_texture[side]);

	//if (this->gamma == 1.0f) { // simple case: no need for shader
	//	ID3D11Resource* render_target = NULL;
	//	this->d3d.view[side]->GetResource(&render_target);
	//	ID3D11Resource* input_texture = (ID3D11Resource*)d3d.texture[side];

	//	this->d3d.context->CopyResource(render_target, input_texture);
	//	render_target->Release();
	//}
	//else { // else: use the shader
	//	HRESULT hr;
	//	ID3D11Resource* input_texture = (ID3D11Resource*)d3d.texture[side];
	//	// reset D3D
	//	this->d3d.context->ClearState();

	//	ID3D11RenderTargetView* render_target_view = this->d3d.view[side];

	//	// Bind the render target view and depth stencil buffer to the output render pipeline.
	//	this->d3d.context->OMSetRenderTargets(1, &render_target_view, 0);

	//	// Setup the viewport for rendering.
	//	D3D11_VIEWPORT viewport;
	//	viewport.TopLeftX = 0.0f;
	//	viewport.TopLeftY = 0.0f;
	//	viewport.Width = (float)this->texture_width;
	//	viewport.Height = (float)this->texture_height;
	//	viewport.MinDepth = 0.0f;
	//	viewport.MaxDepth = 1.0f;
	//	// Create the viewport.
	//	this->d3d.context->RSSetViewports(1, &viewport);

	//	unsigned int stride = sizeof(D3D::Vertex);
	//	unsigned int offset = 0;
	//	// Set the vertex buffer to active in the input assembler so it can be rendered.
	//	this->d3d.context->IASetVertexBuffers(0, 1, &this->d3d.vertex_buffer, &stride, &offset);
	//	// Set the index buffer to active in the input assembler so it can be rendered.
	//	this->d3d.context->IASetIndexBuffer(this->d3d.index_buffer, DXGI_FORMAT_R32_UINT, 0);
	//	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	//	this->d3d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//	// Now set the rasterizer state.
	//	this->d3d.context->RSSetState(this->d3d.rasterizer_state);

	//	ID3D11ShaderResourceView* resource_view;
	//	hr = this->d3d.device->CreateShaderResourceView(input_texture, NULL, &resource_view);
	//	this->d3d.context->PSSetShaderResources(0, 1, &resource_view);

	//	// Set shaders
	//	this->d3d.context->IASetInputLayout(this->d3d.input_layout);
	//	this->d3d.context->VSSetShader(this->d3d.vertex_shader, NULL, 0);
	//	this->d3d.context->PSSetShader(this->d3d.pixel_shader, NULL, 0);
	//	this->d3d.context->PSSetSamplers(0, 1, &this->d3d.sampler_state);

	//	// Set gamma
	//	D3D11_MAPPED_SUBRESOURCE mapped_resource;
	//	hr = this->d3d.context->Map(this->d3d.param_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
	//	DirectX::XMFLOAT4* dst = (DirectX::XMFLOAT4*)mapped_resource.pData;
	//	dst->x = aperture_u;
	//	dst->y = aperture_v;
	//	dst->z = 1.0f / this->gamma;
	//	this->d3d.context->Unmap(this->d3d.param_buffer, 0);
	//	ID3D11Buffer* ps_buffers[1] = {
	//		this->d3d.param_buffer
	//	};
	//	this->d3d.context->VSSetConstantBuffers(0, 1, ps_buffers);
	//	this->d3d.context->PSSetConstantBuffers(0, 1, ps_buffers);

	//	// Render.
	//	this->d3d.context->DrawIndexed(4, 0, 0);

	//	resource_view->Release();
	//}
#endif
#endif

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     blitEyes()
/**
 * Submit rendered images into the internal eye textures.
  * TODO_XR: aperture_u and aperture_v currently don't do anything in the shader.
 */
int VR_OpenXR::blitEyes(void* texture_resource_left, void* texture_resource_right, const float& aperture_u, const float& aperture_v)
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	wglDXLockObjectsNV(shared_device, 2, &shared_texture[0]);
#endif
#endif

	uint texture_id_left = *((uint*)texture_resource_left);
	uint texture_id_right = *((uint*)texture_resource_right);

	// Save previous OpenGL state
	GLint prior_framebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prior_framebuffer);
	GLint prior_program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prior_program);
	GLboolean prior_backface_culling = glIsEnabled(GL_CULL_FACE);
	GLboolean prior_blend_enabled = glIsEnabled(GL_BLEND);
	GLboolean prior_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean prior_texture_enabled = glIsEnabled(GL_TEXTURE_2D);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	glUseProgram(this->gl.program);

	// Load shader params
	glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0f / this->gamma, 0);
	glBindVertexArray(this->gl.vertex_array);
	glEnableVertexAttribArray(this->gl.position_location);
	glEnableVertexAttribArray(this->gl.uv_location);

	for (int i = 0; i < 2; ++i) {
		// Bind my render buffer as render target eye render target
		glBindFramebuffer(GL_FRAMEBUFFER, this->gl.framebuffer[i]);
		glViewport(0, 0, this->texture_width, this->texture_height);
		glClearColor(0, 0, 0, 0);
		/* TODO_XR: Clearing the color buffer bit causes crash on AMD Vega64. */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glUniform4f(this->gl.param_location, aperture_u, aperture_v, 1.0f / this->gamma, 0);

		// Render the provided texture into the HMD eye texture.
		if (i == Side_Left) {
			glBindTexture(GL_TEXTURE_2D, texture_id_left);
		}
		else {
			glBindTexture(GL_TEXTURE_2D, texture_id_right);
		}

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glDisableVertexAttribArray(this->gl.position_location);
	glDisableVertexAttribArray(this->gl.uv_location);

	// Restore previous OpenGL state
	glUseProgram(prior_program);
	prior_backface_culling ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
	prior_blend_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	prior_depth_test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
	prior_texture_enabled ? glEnable(GL_TEXTURE_2D) : glDisable(GL_TEXTURE_2D);
	glBindFramebuffer(GL_FRAMEBUFFER, prior_framebuffer);

#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
	wglDXUnlockObjectsNV(shared_device, 2, &shared_texture[0]);

	//ID3D11Resource* input_textures[Sides] =
	//{ (ID3D11Resource*)d3d.texture[Side_Left],
	//  (ID3D11Resource*)d3d.texture[Side_Right] };
	//
	//if (this->gamma == 1.0f) { // simple case: no need for shader
	//	for (int i = 0; i < 2; ++i) {
	//		ID3D11Resource* render_target;
	//		this->d3d.view[i]->GetResource(&render_target);
	//		this->d3d.context->CopyResource(render_target, input_textures[i]);
	//		render_target->Release();
	//	}
	//}
	//else { // else: use the shader
	//	HRESULT hr;
	//	// reset D3D
	//	this->d3d.context->ClearState();
	//
	//	// Setup the viewport for rendering.
	//	D3D11_VIEWPORT viewport;
	//	viewport.TopLeftX = 0.0f;
	//	viewport.TopLeftY = 0.0f;
	//	viewport.Width = (float)this->texture_width;
	//	viewport.Height = (float)this->texture_height;
	//	viewport.MinDepth = 0.0f;
	//	viewport.MaxDepth = 1.0f;
	//	// Create the viewport.
	//	this->d3d.context->RSSetViewports(1, &viewport);
	//
	//	unsigned int stride = sizeof(D3D::Vertex);
	//	unsigned int offset = 0;
	//	// Set the vertex buffer to active in the input assembler so it can be rendered.
	//	this->d3d.context->IASetVertexBuffers(0, 1, &this->d3d.vertex_buffer, &stride, &offset);
	//	// Set the index buffer to active in the input assembler so it can be rendered.
	//	this->d3d.context->IASetIndexBuffer(this->d3d.index_buffer, DXGI_FORMAT_R32_UINT, 0);
	//	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	//	this->d3d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//
	//	// Now set the rasterizer state.
	//	this->d3d.context->RSSetState(this->d3d.rasterizer_state);
	//
	//	// Set shaders
	//	this->d3d.context->IASetInputLayout(this->d3d.input_layout);
	//	this->d3d.context->VSSetShader(this->d3d.vertex_shader, NULL, 0);
	//	this->d3d.context->PSSetShader(this->d3d.pixel_shader, NULL, 0);
	//	this->d3d.context->PSSetSamplers(0, 1, &this->d3d.sampler_state);
	//
	//	// Set gamma
	//	D3D11_MAPPED_SUBRESOURCE mapped_resource;
	//	hr = this->d3d.context->Map(this->d3d.param_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
	//	DirectX::XMFLOAT4* dst = (DirectX::XMFLOAT4*)mapped_resource.pData;
	//	dst->x = aperture_u;
	//	dst->y = aperture_v;
	//	dst->z = 1.0f / this->gamma;
	//	this->d3d.context->Unmap(this->d3d.param_buffer, 0);
	//	ID3D11Buffer* ps_buffers[1] = {
	//		this->d3d.param_buffer
	//	};
	//	this->d3d.context->VSSetConstantBuffers(0, 1, ps_buffers);
	//	this->d3d.context->PSSetConstantBuffers(0, 1, ps_buffers);
	//
	//	// Render.
	//	for (int i = 0; i < 2; ++i) {
	//		ID3D11RenderTargetView* render_target_view = this->d3d.view[i];
	//
	//		// Bind the render target view and depth stencil buffer to the output render pipeline.
	//		this->d3d.context->OMSetRenderTargets(1, &render_target_view, 0);
	//
	//		ID3D11ShaderResourceView* resource_view;
	//		hr = this->d3d.device->CreateShaderResourceView(input_textures[i], NULL, &resource_view);
	//		this->d3d.context->PSSetShaderResources(0, 1, &resource_view);
	//
	//		this->d3d.context->DrawIndexed(4, 0, 0);
	//			
	//		resource_view->Release();
	//	}
	//}
#endif
#endif

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    renderLayer()
/**
 * Helper function to render compositor layer.
 */
bool VR_OpenXR::renderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
	XrCompositionLayerProjection& layer)
{
	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	uint32_t viewCapacityInput = (uint32_t)m_views.size();
	uint32_t viewCountOutput;

	XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	viewLocateInfo.displayTime = predictedDisplayTime;
	viewLocateInfo.space = m_appSpace;
	if (XR_FAILED(xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput,
		&viewCountOutput, m_views.data()))) {
		return false;
	}

	if (viewCountOutput == viewCapacityInput && viewCountOutput == m_configViews.size() 
		&& viewCountOutput == m_swapchains.size()) {

		projectionLayerViews.resize(viewCountOutput);

		// Render view to the appropriate part of the swapchain image.
		for (uint32_t i = 0; i < viewCountOutput; ++i) {
			// Each view has a separate swapchain which is acquired, rendered to, and released.
			const Swapchain viewSwapchain = m_swapchains[i];

			XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
			uint32_t swapchainImageIndex;
			if (XR_FAILED(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex))) {
				return false;
			}

			XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
			waitInfo.timeout = XR_INFINITE_DURATION;
			if (XR_FAILED(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo))) {
				return false;
			}

			projectionLayerViews[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
			projectionLayerViews[i].pose = m_views[i].pose;
			projectionLayerViews[i].fov = m_views[i].fov;
			projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
			projectionLayerViews[i].subImage.imageRect.offset = { 0, 0 };
			projectionLayerViews[i].subImage.imageRect.extent = { viewSwapchain.width, viewSwapchain.height };

			XrSwapchainImageBaseHeader* const swapchainImage =
				m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

			// submit the GL / D3D texture that was blitted in blitEye()
#ifdef _WIN32
#if XR_USE_GRAPHICS_API_D3D11
			ID3D11Resource *input_texture = NULL;
			d3d.view[i]->GetResource(&input_texture);
			ID3D11Resource *output_texture = (ID3D11Resource*)reinterpret_cast<XrSwapchainImageD3D11KHR*>(swapchainImage)->texture;
			if (input_texture && output_texture) {
				d3d.context->CopyResource(output_texture, input_texture);
			}
#else
			GLenum input_texture = (GLenum)gl.texture[i];
			GLenum output_texture = (GLenum)reinterpret_cast<XrSwapchainImageOpenGLKHR*>(swapchainImage)->image;
			glCopyBufferSubData(input_texture, output_texture, 0, 0, sizeof(GLubyte) * texture_width * texture_height * 4);
#endif
#else
			GLenum input_texture = (GLenum)gl.texture[i];
			GLenum output_texture = (GLenum)reinterpret_cast<XrSwapchainImageOpenGLKHR*>(swapchainImage)->image;
			glCopyBufferSubData(input_texture, output_texture, 0, 0, sizeof(GLubyte) * texture_width * texture_height * 4);
#endif
			XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
			if (XR_FAILED(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo))) {
				return false;
			}
		}

		layer.space = m_appSpace;
		layer.viewCount = (uint32_t)projectionLayerViews.size();
		layer.views = projectionLayerViews.data();
		return true;
	}
	else {
		return false;
	}
}

//                                                                          ________________________
//_________________________________________________________________________/    submitFrame()
/**
 * Submit frame to device / screen.
 */
int VR_OpenXR::submitFrame()
{
	if (!this->initialized) {
		return VR::Error_NotInitialized;
	}

	if (!m_instance || !m_session) {
		return VR::Error_NotInitialized;
	}

	XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
	//XrFrameState frameState{ XR_TYPE_FRAME_STATE };
	if (XR_FAILED(xrWaitFrame(m_session, &frameWaitInfo, &m_frameState))) {
		return VR::Error_InternalFailure;
	}

	XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
	if (XR_FAILED(xrBeginFrame(m_session, &frameBeginInfo))) {
		return VR::Error_InternalFailure;
	}

	std::vector<XrCompositionLayerBaseHeader*> layers;
	XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
	if (renderLayer(m_frameState.predictedDisplayTime, projectionLayerViews, layer)) {
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
	}

	XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
	frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
	frameEndInfo.environmentBlendMode = m_environmentBlendMode;
	frameEndInfo.layerCount = (uint32_t)layers.size();
	frameEndInfo.layers = layers.data();
	if (XR_FAILED(xrEndFrame(m_session, &frameEndInfo))) {
		return VR::Error_InternalFailure;
	}

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeTexSize
/**
 * Get the default eye texture size.
 */
int VR_OpenXR::getDefaultEyeTexSize(uint& w, uint& h, Side side)
{
	if (!m_instance || !m_session) {
		int error = this->acquireHMD();
		if (error) {
			this->releaseHMD();
			return VR::Error_NotInitialized;
		}
	}

	if (m_configViews.size() < 1) {
		return VR::Error_NotInitialized;
	}

	w = m_configViews[0].recommendedImageRectWidth;
	h = m_configViews[0].recommendedImageRectHeight;

	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/  getDefaultEyeParams
/**
 * Get the HMD's default parameters.
 */
int VR_OpenXR::getDefaultEyeParams(Side side, float& fx, float& fy, float& cx, float& cy)
{
	if (!m_instance || !m_session) {
		int error = this->acquireHMD();
		if (error) {
			this->releaseHMD();
			return VR::Error_NotInitialized;
		}
	}

	XrFovf& fov = m_views[side].fov;

	// fov.UpTan   =     cy    / fy;
	// fov.DownTan = (1.0f-cy) / fy;
	// fov.LeftTan =     cx    / fx;
	// fov.RightTan= (1.0f-cx) / fx;

	float upTan = tanf(fov.angleUp);
	float downTan = tanf(fov.angleDown);
	float leftTan = tanf(fov.angleLeft);
	float rightTan = tanf(fov.angleRight);

	// => fy == cy / fov.UpTan == (1-cy) / fov.DownTan
	// => (1-cy) / cy  ==  fov.DownTan / fov.UpTan  ==  1/cy - cy/cy  ==  1/cy - 1 
	// => 1/cy = (fov.DownTan / fov.UpTan) + 1
	cy = 1.0f / (fabsf(downTan / upTan) + 1.0f);
	fy = cy / upTan;

	// => fx  ==  cx / fov.LeftTan  ==  (1-cx) / fov.RightTan
	// => (1-cx) / cx  ==  fov.RightTan / fov.LeftTan  ==  1/cx - cx/cx  ==  1/cx - 1
	// => 1/cx  ==  (fov.RightTan / fov.LeftTan) + 1
	cx = 1.0f / (fabsf(rightTan / leftTan) + 1.0f);
	fx = -cx / leftTan;

	return VR_OpenXR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/     setEyeParams()
/**
 * Set the HMD's projection parameters.
 * For correct distortion rendering and possibly other internal things,
 * the HMD might need to know these.
 * \param   side    Side of the eye which to set.
 * \param   fx      Horizontal focal length, in "image-width"-units (1=image width).
 * \param   fy      Vertical focal length, in "image-height"-units (1=image height).
 * \param   cx      Horizontal principal point, in "image-width"-units (0.5=image center).
 * \param   cy      Vertical principal point, in "image-height"-units (0.5=image center).
 */
int VR_OpenXR::setEyeParams(Side side, float fx, float fy, float cx, float cy)
{
	/* TODO_XR */
	return VR::Error_None;
}

//                                                                          ________________________
//_________________________________________________________________________/    setEyeOffset
/**
 * Override the offset of the eyes (camera positions) relative to the HMD.
 */
int VR_OpenXR::setEyeOffset(Side side, float x, float y, float z)
{
	if (side != Side_Left && side != Side_Right) {
		return VR::Error_InvalidParameter;
	}

	// store the eye-to-hmd offset
	t_hmd2eye[side][0][0] = 1;   t_hmd2eye[side][0][1] = 0;   t_hmd2eye[side][0][2] = 0;   t_hmd2eye[side][0][3] = 0;
	t_hmd2eye[side][1][0] = 0;   t_hmd2eye[side][1][1] = 1;   t_hmd2eye[side][1][2] = 0;   t_hmd2eye[side][1][3] = 0;
	t_hmd2eye[side][2][0] = 0;   t_hmd2eye[side][2][1] = 0;   t_hmd2eye[side][2][2] = 1;   t_hmd2eye[side][2][3] = 0;
	t_hmd2eye[side][3][0] = x;   t_hmd2eye[side][3][1] = y;   t_hmd2eye[side][3][2] = z;   t_hmd2eye[side][3][3] = 1;
	this->eye_offset_override[side] = true; // don't use defaults anymore

	return VR_OpenXR::Error_None;
}

/***********************************************************************************************//**
*								Exported shared library functions.								   *
***************************************************************************************************/
VR_OpenXR* c_obj(0);

/**
 * Create an object internally. Must be called before the functions below.
 */
int c_createVR()
{
	c_obj = new VR_OpenXR();
	return 0;
}

/**
 * Initialize the internal object (OpenGL).
 */
#ifdef _WIN32
int c_initVR(void* device, void* context)
#else
int c_initVR(void* display, void* drawable, void* context)
#endif
{
#ifdef _WIN32
	return c_obj->init(device, context);
#else
	return c_obj->init(display, drawable, context);
#endif
}

/**
 * Get the type of HMD used for VR.
 */
int c_getHMDType(int* type)
{
	*type = c_obj->hmdType();
	return 0;
}

/**
 * Get the default eye texture size.
 * \param side      Zero for left, one for right., -1 for both eyes (default).
 */
int c_getDefaultEyeTexSize(int* w, int* h, int side)
{
	return c_obj->getDefaultEyeTexSize(*(uint*)w, *(uint*)h, (VR::Side)side);
}

/**
 * Get the HMD's default parameters.
 * \param side       Zero for left, one for right.
 */
int c_getDefaultEyeParams(int side, float* fx, float* fy, float* cx, float* cy)
{
	return c_obj->getDefaultEyeParams((VR::Side)side, *fx, *fy, *cx, *cy);
}

/**
 * Set rendering parameters.
 * \param side      Zero for left, one for right.
 */
int c_setEyeParams(int side, float fx, float fy, float cx, float cy)
{
	return c_obj->setEyeParams((VR::Side)side, fx, fy, cx, cy);
}

/**
 * Update the t_eye positions based on latest tracking data.
 */
int c_updateTrackingVR()
{
	return c_obj->updateTracking();
}

/**
 * Last tracked position of the eyes.
 */
int c_getEyePositions(float t_eye[VR::Sides][4][4])
{
	memcpy(t_eye, c_obj->t_eye, sizeof(float) * VR::Sides * 4 * 4);
	return 0;
}

/**
 *  Last tracked position of the HMD.
 */
int c_getHMDPosition(float t_hmd[4][4])
{
	memcpy(t_hmd, c_obj->t_hmd, sizeof(float) * 4 * 4);
	return 0;
}

/**
 * Last tracked position of the controllers.
 */
int c_getControllerPositions(float t_controller[VR_MAX_CONTROLLERS][4][4])
{
	for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
		if (c_obj->controller[i].available) {
			memcpy(t_controller[i], c_obj->t_controller[i], sizeof(float) * 4 * 4);
		}
	}
	return 0;
}

/**
 * Last tracked button state of the controllers.
 */
int c_getControllerStates(void* controller_states[VR_MAX_CONTROLLERS])
{
	for (int i = 0; i < VR_MAX_CONTROLLERS; ++i) {
		if (c_obj->controller[i].available) {
			memcpy(controller_states[i], &c_obj->controller[i], sizeof(VR::Controller));
		}
		else {
			// Just copy side and availability information.
			memcpy(controller_states[i], &c_obj->controller[i], sizeof(VR::Side) + sizeof(bool));
		}
	}

	return 0;
}

/**
 * Blit a rendered image into the internal eye texture.
 * \param side      Zero for left, one for right.
 */
int c_blitEye(int side, void* texture_resource, const float* aperture_u, const float* aperture_v)
{
	return c_obj->blitEye((VR::Side)side, texture_resource, *aperture_u, *aperture_v);
}

/**
 * Blit rendered images into the internal eye textures.
 */
int c_blitEyes(void* texture_resource_left, void* texture_resource_right, const float* aperture_u, const float* aperture_v)
{
	return c_obj->blitEyes(texture_resource_left, texture_resource_right, *aperture_u, *aperture_v);
}

/**
 * Submit frame to the HMD.
 */
int c_submitFrame()
{
	return c_obj->submitFrame();
}

/**
 * Un-initialize the internal object.
 */
int c_uninitVR()
{
	if (c_obj) {
		int error = c_obj->uninit();
		delete c_obj;
		c_obj = 0;
		return error;
	}
	return 0;
}
