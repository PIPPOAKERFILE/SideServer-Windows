//
//  Application.cpp
//  AltSign-Windows
//
//  Created by Riley Testut on 8/12/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

#include "Application.hpp"

#include "Error.hpp"
#include "ldid.hpp"

#include <fstream>
#include <filesystem>
#include <WinSock2.h>

#define odslog(msg) { std::stringstream ss; ss << msg << std::endl; OutputDebugStringA(ss.str().c_str()); }

extern std::vector<unsigned char> readFile(const char* filename);

namespace fs = std::filesystem;

Device::Type DeviceTypeFromUIDeviceFamily(int deviceFamily)
{
	switch (deviceFamily)
	{
	case 1: return Device::Type::iPhone;
	case 2: return Device::Type::iPad;
	case 3: return Device::Type::AppleTV;
	default: return Device::Type::None;
	}
}

Application::Application() : _minimumOSVersion(0, 0, 0)
{
}

Application::~Application()
{
	for (auto& pair : _entitlements)
	{
		plist_free(pair.second);
	}
}

Application::Application(const Application& app) : _minimumOSVersion(0, 0, 0)
{
	_name = app.name();
	_bundleIdentifier = app.bundleIdentifier();
	_version = app.version();
	_path = app.path();
	_minimumOSVersion = app.minimumOSVersion();
	_supportedDeviceTypes = app.supportedDeviceTypes();

	// Don't assign _entitlementsString or _entitlements,
	// since each copy will create its own entitlements lazily.
	// Otherwise there might be duplicate frees in deconstructor.
}

Application& Application::operator=(const Application& app)
{
	if (this == &app)
	{
		return *this;
	}

	_name = app.name();
	_bundleIdentifier = app.bundleIdentifier();
	_version = app.version();
	_path = app.path();
	_minimumOSVersion = app.minimumOSVersion();
	_supportedDeviceTypes = app.supportedDeviceTypes();

	return *this;
}

Application::Application(std::string appBundlePath) : _minimumOSVersion(0, 0, 0)
{
    fs::path path(appBundlePath);
    path.append("Info.plist");

	auto plistData = readFile(path.string().c_str());

    plist_t plist = nullptr;
    plist_from_memory((const char *)plistData.data(), (int)plistData.size(), &plist);
    if (plist == nullptr)
    {
        throw SignError(SignErrorCode::InvalidApp);
    }

	// Required properties
	auto nameNode = plist_dict_get_item(plist, "CFBundleDisplayName");
	if (nameNode == nullptr)
	{
		nameNode = plist_dict_get_item(plist, "CFBundleName");
	}
	
    auto bundleIdentifierNode = plist_dict_get_item(plist, "CFBundleIdentifier");

    if (nameNode == nullptr || bundleIdentifierNode == nullptr)
    {
		plist_free(plist);
        throw SignError(SignErrorCode::InvalidApp);
    }

	char* name = nullptr;
	plist_get_string_val(nameNode, &name);

	char* bundleIdentifier = nullptr;
	plist_get_string_val(bundleIdentifierNode, &bundleIdentifier);

	// Optional properties
	auto versionNode = plist_dict_get_item(plist, "CFBundleShortVersionString");
	auto minimumOSVersionNode = plist_dict_get_item(plist, "MinimumOSVersion");
	auto deviceFamilyNode = plist_dict_get_item(plist, "UIDeviceFamily");

	std::string version("1.0");
	if (versionNode != nullptr)
	{
		char* versionString = nullptr;
		plist_get_string_val(versionNode, &versionString);
		version = versionString;

		free(versionString);
	}

	OperatingSystemVersion minimumOSVersion(1, 0, 0);
	if (minimumOSVersionNode != nullptr)
	{
		char* minOSVersionString = nullptr;
		plist_get_string_val(minimumOSVersionNode, &minOSVersionString);
		minimumOSVersion = OperatingSystemVersion(minOSVersionString);

		free(minOSVersionString);
	}

	Device::Type supportedDeviceTypes(Device::Type::iPhone);
	if (deviceFamilyNode != nullptr)
	{
		if (PLIST_IS_UINT(deviceFamilyNode))
		{
			uint64_t deviceFamily = 0;
			plist_get_uint_val(deviceFamilyNode, &deviceFamily);

			supportedDeviceTypes = DeviceTypeFromUIDeviceFamily(deviceFamily);
		}
		else if (PLIST_IS_ARRAY(deviceFamilyNode) && plist_array_get_size(deviceFamilyNode) > 0)
		{
			int rawDeviceTypes = Device::Type::None;

			for (int i = 0; i < plist_array_get_size(deviceFamilyNode); i++)
			{
				auto node = plist_array_get_item(deviceFamilyNode, i);
				
				uint64_t deviceFamily = 0;
				plist_get_uint_val(node, &deviceFamily);

				Device::Type deviceType = DeviceTypeFromUIDeviceFamily(deviceFamily);
				rawDeviceTypes |= (int)deviceType;
			}

			supportedDeviceTypes = (Device::Type)rawDeviceTypes;
		}
	}

    _name = name;
    _bundleIdentifier = bundleIdentifier;
    _version = version;
    _path = appBundlePath;
	_minimumOSVersion = minimumOSVersion;
	_supportedDeviceTypes = supportedDeviceTypes;

	free(name);
	free(bundleIdentifier);

	plist_free(plist);
}


#pragma mark - Description -

std::ostream& operator<<(std::ostream& os, const Application& app)
{
    os << "Name: " << app.name() << " ID: " << app.bundleIdentifier();
    return os;
}
    
#pragma mark - Getters -
    
std::string Application::name() const
{
    return _name;
}

std::string Application::bundleIdentifier() const
{
    return _bundleIdentifier;
}

std::string Application::version() const
{
    return _version;
}

std::string Application::path() const
{
    return _path;
}

std::shared_ptr<ProvisioningProfile> Application::provisioningProfile()
{
	if (_provisioningProfile == NULL)
	{
		fs::path path(this->path());
		path.append("embedded.mobileprovision");

		_provisioningProfile = std::make_shared<ProvisioningProfile>(path.string());
	}

	return _provisioningProfile;
}

std::vector<std::shared_ptr<Application>> Application::appExtensions() const
{
	std::vector<std::shared_ptr<Application>> appExtensions;

	fs::path plugInsPath(this->path());
	plugInsPath.append("PlugIns");

	if (!fs::exists(plugInsPath))
	{
		return appExtensions;
	}

	for (auto& file : fs::directory_iterator(plugInsPath))
	{
		if (file.path().extension() != ".appex")
		{
			continue;
		}

		auto appExtension = std::make_shared<Application>(file.path().string());
		if (appExtension == nullptr)
		{
			continue;
		}

		appExtensions.push_back(appExtension);
	}

	return appExtensions;
}

OperatingSystemVersion Application::minimumOSVersion() const
{
	return _minimumOSVersion;
}

Device::Type Application::supportedDeviceTypes() const
{
	return _supportedDeviceTypes;
}

std::string Application::entitlementsString()
{
	if (_entitlementsString == "")
	{
		_entitlementsString = ldid::Entitlements(this->path());
	}

	return _entitlementsString;
}

std::map<std::string, plist_t> Application::entitlements()
{
	if (_entitlements.size() == 0)
	{
		auto rawEntitlements = this->entitlementsString();

		plist_t plist = nullptr;
		plist_from_memory((const char*)rawEntitlements.data(), (int)rawEntitlements.size(), &plist);

		if (plist != nullptr)
		{
			std::map<std::string, plist_t> entitlements;
			char* key = NULL;
			plist_t node = NULL;

			plist_dict_iter it = NULL;
			plist_dict_new_iter(plist, &it);
			plist_dict_next_item(plist, it, &key, &node);

			while (node != nullptr)
			{
				entitlements[key] = plist_copy(node);

				node = NULL;
				free(key);
				key = NULL;
				plist_dict_next_item(plist, it, &key, &node);
			}

			free(it);
			plist_free(plist);

			_entitlements = entitlements;
		}
		else
		{
			odslog("Error parsing entitlements:\n" << rawEntitlements);
		}
	}

	return _entitlements;
}

bool Application::isAltStoreApp() const
{
	auto isAltStoreApp = this->bundleIdentifier().find("com.SideStore.SideStore") != std::string::npos;
	return isAltStoreApp;
}