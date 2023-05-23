//
//  ServerError.hpp
//  AltServer-Windows
//
//  Created by Riley Testut on 8/13/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

#ifndef ServerError_hpp
#define ServerError_hpp

#include "Error.hpp"

#include <sstream>

extern std::string UnderlyingErrorDomainErrorKey;
extern std::string UnderlyingErrorCodeErrorKey;
extern std::string ProvisioningProfileBundleIDErrorKey;
extern std::string AppNameErrorKey;
extern std::string DeviceNameErrorKey;
extern std::string OperatingSystemNameErrorKey;
extern std::string OperatingSystemVersionErrorKey;

extern std::string NSFilePathErrorKey;

enum class ServerErrorCode
{
	UnderlyingError = -1,

    Unknown = 0,
    ConnectionFailed = 1,
    LostConnection = 2,
    
    DeviceNotFound = 3,
    DeviceWriteFailed = 4,
    
    InvalidRequest = 5,
    InvalidResponse = 6,
    
    InvalidApp = 7,
    InstallationFailed = 8,
    MaximumFreeAppLimitReached = 9,
	UnsupportediOSVersion = 10,

	UnknownRequest = 11,
	UnknownResponse = 12,

	InvalidAnisetteData = 13,
	PluginNotFound = 14,

	ProfileNotFound = 15,

	AppDeletionFailed = 16,

	RequestedAppNotRunning = 100,
	IncompatibleDeveloperDisk = 101,
};

class ServerError: public Error
{
public:
	ServerError(ServerErrorCode code, std::map<std::string, std::any> userInfo = {}) : Error((int)code, userInfo)
	{
	}
    
    virtual std::string domain() const
    {
        return "AltServer.ServerError";
    }

	virtual int displayCode() const
	{
		// We want ServerError codes to start at 2000,
		// but we can't change them without breaking AltServer compatibility.
		// Instead, we just add 2000 when displaying code to user
		// to make it appear as if codes start at 2000 normally.
		int displayCode = 2000 + this->code();
		return displayCode;
	}
    
    virtual std::optional<std::string> localizedFailureReason() const
    {
		switch ((ServerErrorCode)this->code())
		{
		case ServerErrorCode::UnderlyingError:
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				if (underlyingError->localizedFailureReason().has_value())
				{
					return *(underlyingError->localizedFailureReason());
				}
			}
			
			if (this->userInfo().count(UnderlyingErrorCodeErrorKey) > 0)
			{
				auto errorCode = AnyStringValue(this->userInfo()[UnderlyingErrorCodeErrorKey]);

				auto failureReason = "Error code: " + errorCode;
				return failureReason;
			}
			else if (this->userInfo().count(NSLocalizedFailureReasonErrorKey) > 0)
			{
				auto localizedFailureReason = AnyStringValue(this->userInfo()[NSLocalizedFailureReasonErrorKey]);
				return localizedFailureReason;
			}
            else
            {
				return std::nullopt;
            }

		case ServerErrorCode::Unknown:
			return "An unknown error occured.";

		case ServerErrorCode::ConnectionFailed:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				if (underlyingError->localizedFailureReason().has_value())
				{
					return *(underlyingError->localizedFailureReason());
				}
			}

			return "There was an error connecting to the device.";
		}

		case ServerErrorCode::LostConnection:
			return "The connection to AltServer was lost.";

		case ServerErrorCode::DeviceNotFound:
			return "AltServer could not find the device.";

		case ServerErrorCode::DeviceWriteFailed:
			return "AltServer could not write app data to the device.";

		case ServerErrorCode::InvalidRequest:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				if (underlyingError->localizedFailureReason().has_value())
				{
					return *(underlyingError->localizedFailureReason());
				}
			}

			return "AltServer received an invalid request.";
		}

		case ServerErrorCode::InvalidResponse:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				if (underlyingError->localizedFailureReason().has_value())
				{
					return *(underlyingError->localizedFailureReason());
				}
			}

			return "AltServer sent an invalid response.";
		}

		case ServerErrorCode::InvalidApp:
			return "The app is in an invalid format.";

		case ServerErrorCode::InstallationFailed:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				if (underlyingError->localizedFailureReason().has_value())
				{
					return *(underlyingError->localizedFailureReason());
				}
				else
				{
					return underlyingError->localizedDescription();
				}
			}

			return "An error occured while installing the app.";
		}

		case ServerErrorCode::MaximumFreeAppLimitReached:
            return "You cannot activate more than 3 apps with a non-developer Apple ID.";

		case ServerErrorCode::UnsupportediOSVersion:
		{
			auto osVersion = this->osVersion();
			if (!osVersion.has_value())
			{
				return "Your device must be running iOS 12.2 or later to install AltStore.";
			}

			std::string appName = this->userInfo().count(AppNameErrorKey) > 0 ? AnyStringValue(this->userInfo()[AppNameErrorKey]) : "The app";

			std::string failureReason = appName + " requires " + *osVersion + " or later.";
			return failureReason;
		}

		case ServerErrorCode::UnknownRequest:
			return "SideServer does not support this request.";

		case ServerErrorCode::UnknownResponse:
			return "SideStore received an unknown response from SideServer.";

		case ServerErrorCode::InvalidAnisetteData:
			return "The provided anisette data is invalid.";

		case ServerErrorCode::PluginNotFound:
			return "SideServer could not connect to Mail plug-in.";

		case ServerErrorCode::ProfileNotFound:
			return "The provisioning profile could not be found.";

		case ServerErrorCode::AppDeletionFailed:
			return "An error occured while removing the app.";

		case ServerErrorCode::RequestedAppNotRunning:
		{
			std::string appName = this->userInfo().count(AppNameErrorKey) > 0 ? AnyStringValue(this->userInfo()[AppNameErrorKey]) : "The requested app";
			std::string deviceName = this->userInfo().count(DeviceNameErrorKey) > 0 ? AnyStringValue(this->userInfo()[DeviceNameErrorKey]) : "the device";

			std::ostringstream oss;
			oss << appName << " is not currently running on " << deviceName << ".";

			return oss.str();
		}

		case ServerErrorCode::IncompatibleDeveloperDisk:
		{
			auto osVersion = this->osVersion();

			std::string failureReason = "The disk is incompatible with " + (osVersion.has_value() ? *osVersion : "this device's OS version") + ".";
			return failureReason;
		}
		}

		return std::nullopt;
    }

    virtual std::optional<std::string> localizedRecoverySuggestion() const
    {
        switch ((ServerErrorCode)this->code())
        {
		case ServerErrorCode::UnderlyingError:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				return underlyingError->localizedRecoverySuggestion();
			}
			else
			{
				return std::nullopt;
			}
		}

        case ServerErrorCode::ConnectionFailed:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				if (underlyingError->localizedRecoverySuggestion().has_value())
				{
					return underlyingError->localizedRecoverySuggestion();
				}
			}

			// If there is no underlying error, fall through to ::DeviceNotFound
			[[fallthrough]];
		}

        case ServerErrorCode::DeviceNotFound:
            return "Make sure you have trusted this device with your computer and WiFi sync is enabled.";

        case ServerErrorCode::MaximumFreeAppLimitReached:
            return "Please deactivate a sideloaded app with AltStore in order to install another app. If you're running iOS 13.5 or later, make sure 'Offload Unused Apps' is disabled in Settings > iTunes & App Stores, then install or delete all offloaded apps to prevent them from erroneously counting towards this limit.";

        case ServerErrorCode::InvalidAnisetteData:
            return "Please download the latest versions of iTunes and iCloud directly from Apple, and not from the Microsoft Store.";

        case ServerErrorCode::RequestedAppNotRunning:
        {
            std::string deviceName = this->userInfo().count(DeviceNameErrorKey) > 0 ? AnyStringValue(this->userInfo()[DeviceNameErrorKey]) : "your device";

            std::string localizedRecoverySuggestion = "Make sure the app is running in the foreground on " + deviceName + " then try again.";
            return localizedRecoverySuggestion;
        }

        default: return Error::localizedRecoverySuggestion();
        }
    }

	virtual std::optional<std::string> localizedDebugDescription() const
	{
		switch ((ServerErrorCode)this->code())
		{
		case ServerErrorCode::UnderlyingError:
		case ServerErrorCode::InvalidRequest:
		case ServerErrorCode::InvalidResponse:
		{
			if (this->userInfo().count(NSUnderlyingErrorKey) > 0)
			{
				auto underlyingError = std::any_cast<std::shared_ptr<Error>>(this->userInfo()[NSUnderlyingErrorKey]);
				return underlyingError->localizedDebugDescription();
			}
			else
			{
				break;
			}
		}

		case ServerErrorCode::IncompatibleDeveloperDisk:
		{
			if (this->userInfo().count(NSFilePathErrorKey) == 0)
			{
				break;
			}

			auto path = AnyStringValue(this->userInfo()[NSFilePathErrorKey]);
			auto osVersion = this->osVersion().has_value() ? (*this->osVersion()) : "this device's OS version";

			std::string failureReason = std::string("The Developer disk located at ") + path + " is incompatible with " + osVersion + ".";
			return failureReason;
		}
		}

		return Error::localizedDebugDescription();
	}

private:
	std::optional<std::string> osVersion() const;
};

#endif /* ServerError_hpp */
