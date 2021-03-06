// Copyright (c) 2017, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'dart:io';

import 'package:path/path.dart' as path;
import 'package:usage/src/usage_impl.dart';
import 'package:usage/src/usage_impl_io.dart';
import 'package:usage/src/usage_impl_io.dart' as usage_io show getDartVersion;
import 'package:usage/usage.dart';
import 'package:usage/usage_io.dart';

final String _dartDirectoryName = '.dart';
final String _settingsFileName = 'analytics.json';

/// Dart SDK tools with analytics should display this notice.
///
/// In addition, they should support displaying the analytics' status, and have
/// a flag to toggle analytics. This may look something like:
///
/// `Analytics are currently enabled (and can be disabled with --no-analytics).`
final String analyticsNotice =
    "Dart SDK tools anonymously report feature usage statistics and basic "
    "crash reports to help improve Dart tools over time. See Google's privacy "
    "policy: https://www.google.com/intl/en/policies/privacy/.";

/// Create an [Analytics] instance with the given trackingID and
/// applicationName.
///
/// This analytics instance will share a common enablement state with the rest
/// of the Dart SDK tools.
Analytics createAnalyticsInstance(String trackingId, String applicationName,
    {bool disableForSession: false}) {
  Directory dir = getDartStorageDirectory();
  if (!dir.existsSync()) {
    dir.createSync();
  }

  File file = new File(path.join(dir.path, _settingsFileName));
  return new _TelemetryAnalytics(
      trackingId, applicationName, getDartVersion(), file, disableForSession);
}

/// The directory used to store the analytics settings file.
///
/// Typically, the directory is `~/.dart/` (and the settings file is
/// `analytics.json`).
Directory getDartStorageDirectory() =>
    new Directory(path.join(userHomeDir(), _dartDirectoryName));

/// Return the version of the Dart SDK.
String getDartVersion() => usage_io.getDartVersion();

class _TelemetryAnalytics extends AnalyticsImpl {
  final bool disableForSession;

  _TelemetryAnalytics(
    String trackingId,
    String applicationName,
    String applicationVersion,
    File file,
    this.disableForSession,
  )
      : super(
          trackingId,
          new IOPersistentProperties.fromFile(file),
          new IOPostHandler(),
          applicationName: applicationName,
          applicationVersion: applicationVersion,
        ) {
    final String locale = getPlatformLocale();
    if (locale != null) {
      setSessionValue('ul', locale);
    }
  }

  @override
  bool get enabled {
    if (disableForSession || _isRunningOnBot()) {
      return false;
    }
    return super.enabled;
  }
}

bool _isRunningOnBot() {
  // - https://docs.travis-ci.com/user/environment-variables/
  // - https://www.appveyor.com/docs/environment-variables/
  // - CHROME_HEADLESS and BUILDBOT_BUILDERNAME are properties on Chrome infra
  //   bots.
  return Platform.environment['TRAVIS'] == 'true' ||
      Platform.environment['BOT'] == 'true' ||
      Platform.environment['CONTINUOUS_INTEGRATION'] == 'true' ||
      Platform.environment['CHROME_HEADLESS'] == '1' ||
      Platform.environment.containsKey('BUILDBOT_BUILDERNAME') ||
      Platform.environment.containsKey('APPVEYOR') ||
      Platform.environment.containsKey('CI');
}
