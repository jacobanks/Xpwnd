# Xpwnd
A modded version of Xcode designed specifically anything iOS Security Related. It comes loaded with 2 supersets of the iOS SDK - one geared towards jailbreak development, the other towards iOS security research. Both also include a myriad of custom project templates.
<br><br>More specifically, this repo contains a script for you to build Xpwnd yourself. Doing so has 2 advantages:

- I'm not distributing a massive bundle, so you can more easily verify that nothing fishy is being injected
- Apple can't come after me for distributing a bundle that mostly resembles their copyrighted material

<br>

## Building
> Currently only fully tested with Xcode 10 - Xcode 10.2.1<br>

Simply run the script, sit back, and enter your sudo password a couple of times. The script doesn't take arguments and handles a lot of things automatically

### Requirements
1. Xcode (duh!)
2. Mac Developer Signing Identity (can be verified with `security find-identity -v -p codesigning`)
3. Approximately 30 GB of free storage (after building it only takes up 20 GB)

<br>

## Pros of Xpwnd
- Stop wasting time searching for headers or configuring custom include paths.
- Inherent skid defense: published code can't be compiled with Xcode without major advanced reconfiguring
   - Sure, they could still install this script and build Xpwnd themselves, but with how manual everything is in Xpwnd right now, they'd still have a hard time compiling jack squat
- Red is cooler than Blue

<br>

## Features
I'll fill this out more later, but the big items are SDKs (including mythical Sparse SDKs) for Jailbreak development and iOS Security Research.
These are currently in beta and are continually being added to. I'll maybe add a repo later that includes a script for building these individually.

<br>

## Future
- Tweak development with full Theos support (kinda like iOSOpenDev, but modern and better)
- Switch to `.xpwndproj` project type

<br>

## Bugs
- The modern build system crashes - all projects must be manually switched to the legacy build system
- When linking libraries, LD still searches in the iPhoneOS SDK (I think it has something to do with the `isysroot` flag)
- Derived data is shared with Xcode
- Process Name still appears as Xcode

Other than that, it's surprisingly stable

<br>

## Probably Unfixable
- Cannot copy/past Interface Builder items from Xcode to Xpwnd
- Cannot sign in to new accounts (process is not apple signed) (teams can still be manually added in project settings for signing)
- Provisioning profiles cannot be issued to custom platforms, thus why they are only used for housing things

<br>

## Mitigations
* **Redefinition of `OSTypes.h` and friends** &rarr; Delete `/libkern/` from the corresponding Sparse SDK; Rebuild/Relaunch (if still fails, try deleting from the full SDK instead) - then put it back
* **Build System Crashes** &rarr; Navigate to `File ⟩ Project Settings`; Change `Build System` to `Legacy Build System`
* **LD only searches iPhoneOS SDK** &rarr; Right click your `xcodeproj` file and select "Show Package Contents", then open `project.pbxproj` and search for the library you want to link. Change any instance of its path (there should only be one) to be the full path, rather than a relative one

<br>

## Best Way To Contribute
- If you're a jailbreak dev / security researcher, tell me what headers and libraries you commonly use so I can add them to the SDK
- If you're a tinkerer, maybe poke around and see if you can't fix any issues and tell me what you fixed
- If you're a user, tell me if you like it, what's good, what isn't good, and what issues you encounter so I can add them to the list

<br>

## Contact
Please report all unlisted bugs to the "Issues" page here on GitHub. <br> For everything else, you can contact me here: <br>

Twitter: <br>
[@tomnific](https://www.twitter.com/tomnific "Tom's Twitter") <br>

Email: <br>
[tom@southernderd.us](tom@southernderd.us "Tom's Email") <br>
