#include "authmodule.h"

#include <gtest/gtest.h>

class UT_AuthModule : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    AuthModule *m_authModule;
};

void UT_AuthModule::SetUp()
{
    m_authModule = new AuthModule;
}

void UT_AuthModule::TearDown()
{
    delete m_authModule;
}

TEST_F(UT_AuthModule, BasicTest)
{
    m_authModule->authStatus();
    m_authModule->authType();
    m_authModule->setAnimationState(false);
    m_authModule->setAuthResult(0, "test");
    // m_authModule->setAuthStatus("");
    m_authModule->setLimitsInfo(LimitsInfo());
}
