//
// Application Singleton
//

// Internal private state
const tabViews = new Map();
const activeTabViews = new Map();

export function setTabViewFunc(id, getFunc) {

    tabViews.set(id, getFunc);
}

export function getTabViewFunc(id) {

    return tabViews.get(id);
}

export function isTabViewActive(id) {

    if (activeTabViews.get(id)) {
        return true;;
    }
    return false;
}

export function setTabViewActive(id, state) {

    activeTabViews.set(id, state);
}

